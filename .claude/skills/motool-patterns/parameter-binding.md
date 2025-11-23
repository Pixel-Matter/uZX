# Parameter Binding System

## Overview

MoTool's parameter binding system provides a comprehensive architecture for connecting UI widgets to both persistent ValueTree state AND live automation parameters, with full type safety and MIDI learn support.

## Problem Statement

Traditional parameter systems face challenges:

1. **Type erasure**: Automation uses floats, UI uses various types (int, bool, enum)
2. **Dual state**: Parameters need both stored (ValueTree) and live (automation) values
3. **Thread safety**: Automation runs on audio thread, UI on message thread
4. **Format conversion**: Converting between types, strings, and normalized values
5. **Widget binding**: Connecting sliders/buttons without boilerplate
6. **MIDI learn**: Adding MIDI control to parameters
7. **Undo support**: Integration with UndoManager

## Solution Architecture

### Multi-Layer Architecture

```
┌─────────────────────────────────────────────────┐
│ Widget Layer: SliderParamEndpointBinding        │
│               ButtonParamEndpointBinding        │
└──────────────────────┬──────────────────────────┘
                       │ uses
             ┌─────────▼─────────┐
             │ ParameterEndpoint │ (interface)
             └─────────┬─────────┘
                       │ implemented by
           ┌───────────┴─────────────┐
           │                         │
┌──────────▼──────────┐ ┌────────────▼────────────┐
│ ParamValueEndpoint  │ │ AutomatableParamEndpoint│
└──────────┬──────────┘ └────────────┬────────────┘
           │ adapts                  │ adapts
           │                         │
┌──────────▼──────────┐ ┌────────────▼────────────┐
│   ParameterValue    │ │   BindedAutoParameter   │
└──────────┬──────────┘ └────────────┬────────────┘
           │ uses                    │ inherits
           │                         │
┌──────────▼──────────┐ ┌────────────▼────────────┐
│     CachedValue     │ │   AutomatableParameter  │
│     (ValueTree)     │ │  (Tracktion automation) │
└─────────────────────┘ └─────────────────────────┘
```

**Key Files**:
- `src/controllers/Parameters.h` - ParameterDef, ParameterValue
- `src/controllers/BindedAutoParameter.h` - Automation bridge
- `src/controllers/ParamEndpoint.h` - Widget endpoints
- `src/gui/common/ParamBindings.h` - Widget bindings
- `docs/Parameter binding.md` - Full architecture documentation

## Core Components

### 1. ParameterDef<T> - Metadata

Defines everything about a parameter:

```cpp
template<typename T>
struct ParameterDef {
    juce::Identifier identifier;    // Unique ID (e.g., "cutoff")
    juce::Identifier propertyID;    // ValueTree property (e.g., IDs::cutoff)
    juce::String label;             // Display name
    juce::String description;       // Tooltip text
    juce::String units;             // "Hz", "dB", "%"
    juce::NormalisableRange<T> range;  // Min, max, skew, step
    T defaultValue;
    std::function<juce::String(T, int)> valueToText;
    std::function<T(const juce::String&)> textToValue;
};
```

**File**: `src/controllers/Parameters.h:84-149`

**Usage**:
```cpp
ParameterDef<float> cutoffDef {
    .identifier = "cutoff",
    .propertyID = IDs::cutoff,
    .label = "Cutoff",
    .description = "Filter cutoff frequency",
    .units = "Hz",
    .range = { 20.0f, 20000.0f, 0.0f, 0.3f },  // Skew for log scale
    .defaultValue = 1000.0f
};
```

### 2. ParameterValue<T> - Storage

Bundles definition with `CachedValue<T>` for persistence:

```cpp
template<typename T>
class ParameterValue {
public:
    const ParameterDef<T>& def;

    // Attach to ValueTree
    void referTo(juce::ValueTree state, juce::UndoManager* um) {
        cachedValue.referTo(state, def.propertyID, um, def.defaultValue);
    }

    // Stored value access
    T getStoredValue() const { return cachedValue.get(); }
    void setStoredValue(T value) { cachedValue = value; }

    // Live value access (with optional live reader)
    T getLiveValue() const {
        if (liveReader) return liveReader();
        return getStoredValue();
    }

    // For widget binding
    juce::Value getPropertyAsValue() {
        return cachedValue.getPropertyAsValue();
    }

    // Live reader registration (e.g., from automation)
    void setLiveReader(LiveAccessor reader) { liveReader = reader; }

private:
    juce::CachedValue<T> cachedValue;
    LiveAccessor liveReader;  // Optional live value source
};
```

**File**: `src/controllers/Parameters.h:477-544`

**Pattern**: Stored value in ValueTree, optional live reader for automation

### 3. BindedAutoParameter<T> - Automation Bridge

Connects `ParameterValue<T>` to Tracktion's automation system:

```cpp
template<typename T>
class BindedAutoParameter : public tracktion::AutomatableParameter {
public:
    BindedAutoParameter(
        ParameterValue<T>& pv,
        tracktion::AutomatableEditItem* owner,
        juce::UndoManager* um
    )
        : AutomatableParameter(
            pv.def.identifier.toString(),
            pv.def.label,
            *owner,
            toNormalisable01(pv.def.range)
        )
        , paramValue(pv)
        , undoManager(um)
    {
        // Register as live reader
        paramValue.setLiveReader([this] {
            return getCurrentValue();
        });

        // Sync initial state from ValueTree
        updateFromAttachedParamValue();
    }

    // AutomatableParameter overrides
    T getCurrentValue() const override {
        return fromNormalized(getCurrentNormalisedValue());
    }

    void setParameter(float newValue, juce::NotificationType notification) override {
        AutomatableParameter::setParameter(newValue, notification);

        // Write automation back to ValueTree (async on message thread)
        triggerAsyncUpdate();
    }

    void handleAsyncUpdate() override {
        // Thread-safe: message thread only
        if (undoManager != nullptr) {
            paramValue.setStoredValue(getCurrentValue());
        }
    }

private:
    ParameterValue<T>& paramValue;
    juce::UndoManager* undoManager;

    T fromNormalized(float normalized) const {
        return ParameterConversionTraits<T>::fromNormalized(
            paramValue.def.range, normalized
        );
    }
};
```

**File**: `src/controllers/BindedAutoParameter.h:29-135`

**Key features**:
- Registers itself as live reader in `ParameterValue`
- Syncs automation → ValueTree via `AsyncUpdater`
- Handles type conversion (float ↔ T)

### 4. ParameterEndpoint - Widget Interface

Unified interface for widget bindings:

```cpp
class ParameterEndpoint {
public:
    virtual ~ParameterEndpoint() = default;

    // Value access
    virtual float getStoredFloatValue() const = 0;
    virtual void setStoredFloatValue(float value) = 0;
    virtual float getLiveFloatValue() const = 0;

    // Formatting
    virtual juce::String getCurrentValueAsString() const = 0;
    virtual juce::NormalisableRange<float> getRange() const = 0;

    // Discrete values (for enums/bools)
    virtual int getNumberOfStates() const = 0;
    virtual juce::String getStateLabel(int state) const = 0;

    // Gestures (for automation recording)
    virtual void beginGesture() {}
    virtual void endGesture() {}

    // Change notification
    class Listener {
        virtual void parameterChanged() = 0;
    };
    virtual void addListener(Listener* l) = 0;
    virtual void removeListener(Listener* l) = 0;
};
```

**File**: `src/controllers/ParamEndpoint.h:18-83`

**Implementations**:
- `ParamValueEndpoint<T>`: Wraps `ParameterValue<T>`
- `AutomatableParamEndpoint`: Wraps `AutomatableParameter`

### 5. SliderParamEndpointBinding - Widget Binding

Connects slider to parameter:

```cpp
class SliderParamEndpointBinding : public WidgetParamEndpointBinding {
public:
    SliderParamEndpointBinding(
        juce::Slider& slider,
        std::unique_ptr<ParameterEndpoint> endpoint
    )
        : WidgetParamEndpointBinding(slider, std::move(endpoint))
        , slider(slider)
    {
        // Configure slider from endpoint
        slider.setRange(endpoint->getRange().start,
                        endpoint->getRange().end,
                        endpoint->getRange().interval);

        // Wire callbacks
        slider.onValueChange = [this] {
            endpoint->setStoredFloatValue(slider.getValue());
        };

        slider.onDragStart = [this] { endpoint->beginGesture(); };
        slider.onDragEnd = [this] { endpoint->endGesture(); };

        // Initial sync
        refreshFromEndpoint();
    }

    void parameterChanged() override {
        slider.setValue(endpoint->getLiveFloatValue(), juce::dontSendNotification);
    }

private:
    juce::Slider& slider;
};
```

**File**: `src/gui/common/ParamBindings.h:72-108`

**Features**:
- Auto-configures slider range
- Syncs slider ↔ parameter bidirectionally
- Handles gestures for automation recording
- MIDI learn support (inherited from `WidgetParamEndpointBinding`)

## Usage Examples

### Example 1: Simple Parameter with UI

```cpp
class MyPlugin : public PluginBase {
public:
    // 1. Define parameter
    ParameterDef<float> cutoffDef {
        .identifier = "cutoff",
        .propertyID = IDs::cutoff,
        .label = "Cutoff",
        .range = { 20.0f, 20000.0f },
        .defaultValue = 1000.0f
    };

    ParameterValue<float> cutoff { cutoffDef };

    MyPlugin(tracktion::PluginCreationInfo info)
        : PluginBase(info)
    {
        // 2. Attach to ValueTree
        cutoff.referTo(state, getUndoManager());

        // 3. Add automation (optional)
        auto autoParam = addParam(cutoff, this, getUndoManager());
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
        // 4. Use live value in audio thread
        float cutoffHz = cutoff.getLiveValue();

        // Apply filter at cutoffHz...
    }
};

class MyPluginUI : public PluginDeviceUI {
public:
    MyPluginUI(tracktion::Plugin::Ptr plugin)
        : plugin(dynamic_cast<MyPlugin*>(plugin.get()))
    {
        // 5. Bind slider to parameter
        cutoffSliderBinding = std::make_unique<SliderParamEndpointBinding>(
            cutoffSlider,
            makeResolveParamEndpoint(plugin->cutoff)
        );

        addAndMakeVisible(cutoffSlider);
    }

private:
    MyPlugin* plugin;
    juce::Slider cutoffSlider;
    std::unique_ptr<SliderParamEndpointBinding> cutoffSliderBinding;
};
```

### Example 2: Enum Parameter

```cpp
enum class WaveformType { Sine, Square, Saw, Triangle };

// Define enum choices
using WaveformChoice = Util::EnumChoice<WaveformType>;

ParameterDef<WaveformChoice> waveformDef {
    .identifier = "waveform",
    .propertyID = IDs::waveform,
    .label = "Waveform",
    .range = { 0, 3, 1 },  // 4 discrete values
    .defaultValue = WaveformChoice { WaveformType::Sine, "Sine" }
};

ParameterValue<WaveformChoice> waveform { waveformDef };

// In UI: use ComboBox
comboBoxBinding = std::make_unique<ComboBoxParamEndpointBinding>(
    waveformComboBox,
    makeResolveParamEndpoint(waveform)
);
```

### Example 3: Boolean Parameter

```cpp
ParameterDef<bool> bypassDef {
    .identifier = "bypass",
    .propertyID = IDs::bypass,
    .label = "Bypass",
    .defaultValue = false
};

ParameterValue<bool> bypass { bypassDef };

// In UI: use ToggleButton
buttonBinding = std::make_unique<ButtonParamEndpointBinding>(
    bypassButton,
    makeResolveParamEndpoint(bypass)
);
```

### Example 4: MIDI Learn

MIDI learn is automatically supported through `WidgetParamEndpointBinding`:

```cpp
// Right-click on slider triggers MIDI learn
// (handled by WidgetParamEndpointBinding's MouseListenerWithCallback)

// Manual MIDI learn control:
if (auto binding = dynamic_cast<WidgetParamEndpointBinding*>(sliderBinding.get())) {
    binding->showMidiLearnMenu();  // Show MIDI learn popup
}
```

## Data Flow

### Stored Value Path

```
User adjusts slider
    → SliderParamEndpointBinding::onValueChange
        → ParameterEndpoint::setStoredFloatValue()
            → ParameterValue::setStoredValue()
                → CachedValue::operator=()
                    → ValueTree property updated
                        → Persisted to disk
```

### Live Value Path (with Automation)

```
Automation playback
    → AutomatableParameter::setParameter()
        → BindedAutoParameter::setParameter()
            → AsyncUpdater::triggerAsyncUpdate()
                → (Message thread)
                → BindedAutoParameter::handleAsyncUpdate()
                    → ParameterValue::setStoredValue()
                        → ValueTree updated

Widget reads value:
    → ParameterEndpoint::getLiveFloatValue()
        → ParameterValue::getLiveValue()
            → LiveAccessor() (registered by BindedAutoParameter)
                → BindedAutoParameter::getCurrentValue()
                    → AutomatableParameter::getCurrentNormalisedValue()
```

## Type Safety

### Conversion Traits

`ParameterConversionTraits<T>` handle type-specific conversions:

```cpp
// Float: pass-through
ParameterConversionTraits<float>::toFloat(value) → value

// Int: cast
ParameterConversionTraits<int>::toFloat(value) → (float)value

// Bool: 0.0 or 1.0
ParameterConversionTraits<bool>::toFloat(value) → value ? 1.0f : 0.0f

// Enum: enum value as float
ParameterConversionTraits<EnumChoice<E>>::toFloat(value) → (float)value.value
```

**File**: `src/controllers/Parameters.h:20-82`

### Normalized Range Conversion

Automation expects `[0, 1]` range:

```cpp
// Range [20, 20000] with log skew → [0, 1]
float normalized = range.convertTo0to1(value);

// [0, 1] → [20, 20000]
float denormalized = range.convertFrom0to1(normalized);
```

**Handled automatically** by `BindedAutoParameter`

## Thread Safety

### Rules

1. **Audio thread**: Read `ParameterValue::getLiveValue()` only
2. **Message thread**: Read/write stored values, modify ValueTree
3. **Automation → ValueTree**: Via `AsyncUpdater` (message thread)
4. **UI → Parameter**: Direct on message thread (sliders call from message thread)

### Safe Pattern

```cpp
// In processBlock (audio thread)
void applyToBuffer(const PluginRenderContext& fc) override {
    SCOPED_REALTIME_CHECK

    // SAFE: Read live value (no locks)
    float cutoff = cutoffParam.getLiveValue();

    // UNSAFE: Don't write to ValueTree here!
    // cutoffParam.setStoredValue(newValue);  // DON'T DO THIS
}

// In UI callback (message thread)
void sliderValueChanged(Slider* slider) {
    // SAFE: Write stored value
    cutoffParam.setStoredValue(slider->getValue());
}
```

## Best Practices

### ✅ DO

1. **Define metadata once**: Create `ParameterDef<T>` as class member
2. **Attach in constructor**: Call `referTo()` in plugin constructor
3. **Use live values in audio**: Always `getLiveValue()` in processBlock
4. **Bind with endpoints**: Use `makeResolveParamEndpoint()` for widget binding
5. **Provide defaults**: Always specify `defaultValue` in definition
6. **Add automation**: Use `addParam()` helper for automatable parameters
7. **Clean up bindings**: Store bindings as members, automatic cleanup in destructor

### ❌ DON'T

1. **Access ValueTree in audio thread**: Use live values only
2. **Skip type conversion**: Always use conversion traits for custom types
3. **Forget UndoManager**: Pass to `referTo()` and `addParam()`
4. **Hard-code ranges**: Define in `ParameterDef`
5. **Mix stored/live in UI**: Use endpoints for consistent access
6. **Leak listeners**: Bindings auto-cleanup, but manual listeners need removal

## Advanced Features

### Custom Value Formatting

```cpp
ParameterDef<float> gainDef {
    .identifier = "gain",
    .label = "Gain",
    .range = { -96.0f, 12.0f },
    .defaultValue = 0.0f,
    .valueToText = [](float value, int decimals) {
        return juce::String(value, decimals) + " dB";
    },
    .textToValue = [](const juce::String& text) {
        return text.trimEnd().getFloatValue();
    }
};
```

### Parameter Groups

Organize related parameters:

```cpp
struct FilterParams {
    ParameterValue<float> cutoff;
    ParameterValue<float> resonance;
    ParameterValue<WaveformChoice> type;

    void referTo(ValueTree state, UndoManager* um) {
        cutoff.referTo(state, um);
        resonance.referTo(state, um);
        type.referTo(state, um);
    }
};
```

### Restoring from ValueTree

```cpp
// After loading Edit from disk
void restorePluginStateFromValueTree() {
    for (auto& autoParam : automatable Parameters) {
        autoParam->updateFromAttachedParamValue();
    }
}
```

**File**: `src/controllers/BindedAutoParameter.h:154-175`

## Testing Strategy

```cpp
class ParameterBindingTest : public juce::UnitTest {
public:
    ParameterBindingTest() : UnitTest("Parameter Binding", "MoTool") {}

    void runTest() override {
        beginTest("Stored value persistence");

        ParameterDef<float> def {
            .identifier = "test",
            .propertyID = IDs::testParam,
            .range = { 0.0f, 100.0f },
            .defaultValue = 50.0f
        };

        ParameterValue<float> param { def };

        ValueTree state("PLUGIN");
        param.referTo(state, nullptr);

        // Test default
        expectEquals(param.getStoredValue(), 50.0f);

        // Test set/get
        param.setStoredValue(75.0f);
        expectEquals(param.getStoredValue(), 75.0f);
        expectEquals(state[IDs::testParam], juce::var(75.0f));

        beginTest("Live value with reader");

        // Register live reader
        param.setLiveReader([] { return 100.0f; });

        // Stored != live
        expectEquals(param.getStoredValue(), 75.0f);
        expectEquals(param.getLiveValue(), 100.0f);
    }
};
```

## Performance Characteristics

- **CachedValue access**: `O(1)` - cached, no tree lookup
- **Type conversion**: `O(1)` - inline functions
- **Automation update**: Async, coalesced to one per frame
- **Listener notification**: `O(n)` where n = listener count
- **Memory**: Small overhead per parameter (~64 bytes)

## Related Patterns

- **State Wrappers**: Parameters use same `CachedValue` pattern
- **MIDI Effects**: Use parameter binding for effect parameters
- **Plugin UI Adapters**: UIs bind to plugin parameters

## References

- **Full Documentation**: `docs/Parameter binding.md` (comprehensive architecture guide)
- **Implementation**:
  - `src/controllers/Parameters.h`
  - `src/controllers/BindedAutoParameter.h`
  - `src/controllers/ParamEndpoint.h`
  - `src/gui/common/ParamBindings.h`
- **Examples**: Plugin implementations in `src/plugins/`
- **JUCE ValueTree**: https://docs.juce.com/master/classValueTree.html
- **Tracktion Automation**: See `tracktion-engine` skill documentation
