# Plugin UI Adapter Registry Pattern

## Overview

The Plugin UI Adapter Registry provides weak coupling between generic UI panels (e.g., track device panel) and plugin-specific UI implementations through a type-indexed registry and factory pattern.

## Problem Statement

Creating custom UI for plugins faces several architectural challenges:

1. **Tight coupling**: Track panel would need to know about every plugin type
2. **Extensibility**: Adding new plugins requires modifying track panel code
3. **Dependency direction**: Core UI shouldn't depend on specific plugins
4. **Type safety**: Need compile-time safety when available
5. **Plugin discovery**: Plugins register themselves without central coordinator
6. **Factory abstraction**: Need type-erased factories for runtime lookup

## Solution Architecture

### Registry + Factory Pattern

```
┌─────────────────┐
│ Plugin A        │──┐
│ Plugin B        │──┤
│ Plugin C        │──┤  Register
└─────────────────┘  │  themselves
                     ▼
            ┌─────────────────┐
            │  Registry       │
            │  (Singleton)    │
            └─────────────────┘
                     │  Query by
                     │  plugin ptr
                     ▼
        ┌───────────────────────┐
        │ TrackDevicesPanel     │  (no direct plugin knowledge)
        └───────────────────────┘
```

**Key insight**: Dependency inversion - plugins depend on registry, not vice versa

**File**: `src/gui/devices/PluginUIAdapterRegistry.h`

## Core Components

### 1. PluginUIAdapterRegistry (Singleton)

Central registry mapping plugin types to UI factories:

```cpp
class PluginUIAdapterRegistry {
public:
    // Factory function type
    using UIFactory = std::function<std::unique_ptr<PluginDeviceUI>(tracktion::Plugin::Ptr)>;

    struct AdapterInfo {
        UIFactory factory;
        // Future: could add metadata (name, icon, etc.)
    };

    static PluginUIAdapterRegistry& getInstance();

    // Register adapter by type_index
    void registerAdapter(std::type_index pluginType, const AdapterInfo& adapter);

    // Template registration (compile-time type safety)
    template<typename PluginType, typename UIType>
    void registerAdapter() {
        registerAdapter(std::type_index(typeid(PluginType)), {
            [](tracktion::Plugin::Ptr plugin) -> std::unique_ptr<PluginDeviceUI> {
                return std::make_unique<UIType>(plugin);
            },
        });
    }

    // Create UI for plugin (returns nullptr if no adapter registered)
    std::unique_ptr<PluginDeviceUI> createDeviceUI(tracktion::Plugin::Ptr plugin) const;

private:
    PluginUIAdapterRegistry() = default;
    std::unordered_map<std::type_index, AdapterInfo> adapters_;

    const AdapterInfo* findAdapterInfo(const std::type_info& typeInfo) const;
    const AdapterInfo* findAdapterInfo(const tracktion::Plugin* plugin) const;
};
```

**File**: `src/gui/devices/PluginUIAdapterRegistry.h:19-55`

### 2. PluginUIAdapterRegistrar (RAII Helper)

Auto-registers adapters using static initialization:

```cpp
template<typename PluginType, typename UIType>
class PluginUIAdapterRegistrar {
public:
    PluginUIAdapterRegistrar() {
        PluginUIAdapterRegistry::getInstance().registerAdapter<PluginType, UIType>();
    }
};
```

**File**: `src/gui/devices/PluginUIAdapterRegistry.h:61-69`

**Key feature**: Constructor runs before `main()`, registering plugin UI

### 3. Registration Macro

Convenience macro for one-line registration:

```cpp
#define REGISTER_PLUGIN_UI_ADAPTER(PluginType, UIType) \
    namespace { \
        static inline const PluginUIAdapterRegistrar<PluginType, UIType> \
            plugin_ui_adapter_registrar {}; \
    }
```

**File**: `src/gui/devices/PluginUIAdapterRegistry.h:72-75`

**Usage**: Place in plugin's `.cpp` file for auto-registration

## Implementation Details

### Type Indexing

Uses `std::type_index` for runtime type lookup:

```cpp
void registerAdapter(std::type_index pluginType, const AdapterInfo& adapter) {
    adapters_[pluginType] = adapter;
}

std::unique_ptr<PluginDeviceUI> createDeviceUI(tracktion::Plugin::Ptr plugin) const {
    if (auto info = findAdapterInfo(plugin.get())) {
        return info->factory(plugin);
    }
    return nullptr;  // No adapter registered
}
```

**Benefits**:
- Fast `O(1)` lookup via hash map
- Works with RTTI (requires `-frtti`)
- Type-safe at registration time

### Factory Function Type Erasure

Factory stored as `std::function`:

```cpp
using UIFactory = std::function<std::unique_ptr<PluginDeviceUI>(tracktion::Plugin::Ptr)>;
```

**Trade-offs**:
- **Pro**: Type-erased, can store different factory types
- **Pro**: Captures work naturally (lambdas)
- **Con**: Slight overhead vs function pointer (usually negligible)

### RAII Registration

Static initialization ensures registration before use:

```cpp
// In MyPlugin.cpp
REGISTER_PLUGIN_UI_ADAPTER(MyPlugin, MyPluginUI);

// Static initializer runs before main()
int main() {
    // Registry already populated
    auto& registry = PluginUIAdapterRegistry::getInstance();
    // ...
}
```

**Benefits**:
- No manual registration calls needed
- Plugins self-register on load
- Order-independent (static init before main)

## Usage Examples

### Example 1: Basic Plugin Registration

```cpp
// MyPlugin.h
class MyPlugin : public PluginBase {
public:
    // Plugin implementation...
};

// MyPluginUI.h
class MyPluginUI : public PluginDeviceUI {
public:
    MyPluginUI(tracktion::Plugin::Ptr plugin)
        : PluginDeviceUI(plugin) {}

    void paint(Graphics& g) override {
        // Custom UI rendering
    }
};

// MyPlugin.cpp
#include "MyPlugin.h"
#include "MyPluginUI.h"
#include "PluginUIAdapterRegistry.h"

// Auto-register adapter
REGISTER_PLUGIN_UI_ADAPTER(MyPlugin, MyPluginUI);
```

### Example 2: Dynamic UI Creation in Track Panel

```cpp
class TrackDevicesPanel : public Component {
public:
    void addPluginUI(tracktion::Plugin::Ptr plugin) {
        auto& registry = PluginUIAdapterRegistry::getInstance();

        // Try to create custom UI
        auto ui = registry.createDeviceUI(plugin);

        if (ui != nullptr) {
            // Custom UI available
            addAndMakeVisible(ui.get());
            customUIs.push_back(std::move(ui));
        } else {
            // Fallback to generic UI
            auto genericUI = std::make_unique<GenericPluginUI>(plugin);
            addAndMakeVisible(genericUI.get());
            customUIs.push_back(std::move(genericUI));
        }

        resized();
    }

private:
    std::vector<std::unique_ptr<PluginDeviceUI>> customUIs;
};
```

### Example 3: Complex UI with State Binding

```cpp
class SynthPluginUI : public PluginDeviceUI {
public:
    SynthPluginUI(tracktion::Plugin::Ptr plugin)
        : PluginDeviceUI(plugin)
        , synthPlugin(dynamic_cast<SynthPlugin*>(plugin.get()))
    {
        jassert(synthPlugin != nullptr);

        // Bind UI controls to plugin parameters
        cutoffSlider.setRange(synthPlugin->cutoffParam.def.range);
        cutoffSlider.onValueChange = [this] {
            synthPlugin->cutoffParam.setStoredValue(cutoffSlider.getValue());
        };

        addAndMakeVisible(cutoffSlider);
    }

    void paint(Graphics& g) override {
        // Custom visualization (e.g., waveform, spectrum)
        drawWaveform(g, synthPlugin->getWaveform());
    }

private:
    SynthPlugin* synthPlugin;
    Slider cutoffSlider;

    void drawWaveform(Graphics& g, const std::vector<float>& waveform);
};

// Register
REGISTER_PLUGIN_UI_ADAPTER(SynthPlugin, SynthPluginUI);
```

### Example 4: Conditional Registration

```cpp
// Only register if platform supports advanced UI
#if JUCE_MAC || JUCE_WINDOWS
    REGISTER_PLUGIN_UI_ADAPTER(AdvancedPlugin, AdvancedPluginUI);
#endif

// Fallback to generic UI on other platforms
```

## Advanced Features

### Query Before Creation

Check if adapter exists without creating UI:

```cpp
bool hasCustomUI(tracktion::Plugin::Ptr plugin) const {
    return findAdapterInfo(plugin.get()) != nullptr;
}
```

### Multiple Adapters per Plugin

Register different UIs for same plugin:

```cpp
// Compact view
REGISTER_PLUGIN_UI_ADAPTER(MyPlugin, MyPluginCompactUI);

// Full editor (register with different key)
registry.registerAdapter(
    std::type_index(typeid(MyPlugin)),
    { [](auto p) { return std::make_unique<MyPluginFullUI>(p); } }
);
```

**Challenge**: Current implementation stores one adapter per type

**Solution**: Extend `AdapterInfo` to support multiple UI types:

```cpp
struct AdapterInfo {
    UIFactory compactFactory;
    UIFactory fullFactory;
    // ...
};
```

### Metadata Extension

Extend `AdapterInfo` for richer information:

```cpp
struct AdapterInfo {
    UIFactory factory;
    String displayName;
    Image icon;
    bool supportsResizing;
};
```

## Best Practices

### ✅ DO

1. **Use macro for registration**: `REGISTER_PLUGIN_UI_ADAPTER` in plugin .cpp file
2. **Check for nullptr**: Always handle `createDeviceUI()` returning null
3. **Provide fallback UI**: Generic plugin UI for unregistered types
4. **Register once**: Put registration in plugin implementation file (avoid headers)
5. **Use anonymous namespace**: Macro already does this, keeps registrar private
6. **Type-safe cast**: Use `dynamic_cast` in UI constructor to verify plugin type
7. **Document dependencies**: Note RTTI requirement (`-frtti`)

### ❌ DON'T

1. **Register in headers**: Avoid multiple registration (ODR violation)
2. **Forget nullptr check**: Unregistered plugins return nullptr
3. **Assume RTTI**: Document that pattern requires RTTI enabled
4. **Modify registry at runtime**: Registration should happen at static init
5. **Store raw pointers**: Use `unique_ptr` for ownership
6. **Skip type verification**: Always `dynamic_cast` and assert in UI constructor

## Design Trade-offs

### Singleton Pattern

**Pros**:
- Global access point
- Guaranteed initialization order (lazy singleton)
- Single instance ensures consistency

**Cons**:
- Global state (harder to test)
- Implicit dependencies (hidden coupling)

**Alternative**: Dependency injection (pass registry to consumers)

### Type-Indexed Map

**Pros**:
- Fast lookup `O(1)`
- Type-safe at registration
- Extensible (no enum)

**Cons**:
- Requires RTTI
- Cannot introspect registered types easily
- `type_info` comparisons platform-specific

**Alternative**: String-indexed map (plugin names)

### Static Registration

**Pros**:
- Zero boilerplate for plugin authors
- Automatic registration
- Works with dynamic libraries

**Cons**:
- Hidden side effects (static init)
- Harder to test (global state)
- Link-time dependency issues (may need explicit symbol references)

**Alternative**: Explicit registration in application setup

## Testing Strategy

### Unit Tests

```cpp
class PluginUIAdapterRegistryTest : public juce::UnitTest {
public:
    PluginUIAdapterRegistryTest()
        : UnitTest("PluginUIAdapterRegistry", "MoTool") {}

    void runTest() override {
        beginTest("Register and create UI");

        // Setup
        auto& registry = PluginUIAdapterRegistry::getInstance();

        // Clear registry (test isolation)
        // Note: Current implementation doesn't support clearing
        // Consider adding clear() method for testing

        // Register adapter
        registry.registerAdapter<TestPlugin, TestPluginUI>();

        // Create plugin
        auto engine = createTestEngine();
        auto plugin = createTestPlugin<TestPlugin>(engine);

        // Create UI
        auto ui = registry.createDeviceUI(plugin);

        // Verify
        expect(ui != nullptr);
        expect(dynamic_cast<TestPluginUI*>(ui.get()) != nullptr);

        beginTest("Unregistered plugin returns nullptr");
        auto unregisteredPlugin = createTestPlugin<UnregisteredPlugin>(engine);
        auto nullUI = registry.createDeviceUI(unregisteredPlugin);
        expect(nullUI == nullptr);
    }
};
```

### Integration Tests

Test plugin loading and UI creation in realistic scenarios.

## Performance Characteristics

- **Registration**: `O(1)` hash map insertion at startup
- **Lookup**: `O(1)` hash map lookup
- **Factory call**: `O(1)` function call + UI construction
- **Memory**: `O(n)` where `n` = number of registered plugin types
- **RTTI overhead**: `typeid()` is typically fast (compiler intrinsic)

## Common Pitfalls

### 1. RTTI Disabled

**Problem**: `std::type_index` requires RTTI, compiler errors if `-fno-rtti`

**Solution**: Ensure `-frtti` in build flags (JUCE enables by default)

### 2. Static Init Order Fiasco

**Problem**: Registry accessed before constructed (rare with lazy singleton)

**Solution**: Use lazy initialization in `getInstance()`:
```cpp
static PluginUIAdapterRegistry& getInstance() {
    static PluginUIAdapterRegistry instance;  // Thread-safe in C++11+
    return instance;
}
```

### 3. Symbol Stripping in Dynamic Libraries

**Problem**: Registrars in plugin dylib not called if symbols stripped

**Solution**: Force symbol export or explicit registration call:
```cpp
// In plugin's exported initialization function
extern "C" void initPlugin() {
    PluginUIAdapterRegistry::getInstance()
        .registerAdapter<MyPlugin, MyPluginUI>();
}
```

## Future Enhancements

### Multi-View Support

Support multiple UI modes (compact, full, advanced):

```cpp
enum class UIMode { Compact, Full, Advanced };

std::unique_ptr<PluginDeviceUI> createDeviceUI(
    tracktion::Plugin::Ptr plugin,
    UIMode mode = UIMode::Compact
) const;
```

### Metadata Queries

Query registered plugins without creating UI:

```cpp
std::vector<std::type_index> getRegisteredPluginTypes() const;
AdapterInfo getAdapterInfo(std::type_index type) const;
```

### Category Grouping

Group plugins by category in UI:

```cpp
struct AdapterInfo {
    UIFactory factory;
    String category;  // "Synth", "Effect", "MIDI", etc.
};
```

## Related Patterns

- **Custom Clips**: Similar registry pattern for clip types
- **Plugin UI Adapters**: This pattern enables plugin-specific UIs
- **Parameter Binding**: UIs use parameter binding for controls
- **State Wrappers**: UIs may use state wrappers for view state

## References

- **Implementation**: `src/gui/devices/PluginUIAdapterRegistry.h`
- **Usage**: `src/gui/devices/TrackDevicesPanel.cpp` (or similar)
- **Plugin Base**: `src/controllers/PluginBase.h`
- **JUCE Plugins**: https://docs.juce.com/master/classAudioProcessor.html
- **Tracktion Plugins**: See `tracktion-engine` skill documentation
