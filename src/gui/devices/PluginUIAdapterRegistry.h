#pragma once

#include <JuceHeader.h>
#include <memory>
#include <functional>
#include <unordered_map>
#include <typeindex>

namespace MoTool {

class PluginDeviceUI;
class EditViewState;

/**
 * Plugin UI Adapter Registry provides weak coupling between TrackDevicesPanel
 * and specific plugin UI implementations. Plugins can register their UI factories
 * without TrackDevicesPanel knowing about specific plugin types.
 */
class PluginUIAdapterRegistry {
public:
    // Factory function type for creating device UIs
    using UIFactory = std::function<std::unique_ptr<PluginDeviceUI>(EditViewState&, tracktion::Plugin::Ptr)>;

    // Capability check functions
    // using HasCustomUIFunc = std::function<bool(tracktion::Plugin::Ptr)>;
    // using CanHasPlusButtonFunc = std::function<bool(tracktion::Plugin::Ptr)>;

    // struct AdapterInfo {
    //     UIFactory factory;
    //     HasCustomUIFunc hasCustomUI;
    //     CanHasPlusButtonFunc canHasPlusButton;
    // };

    struct AdapterInfo {
        UIFactory factory;
        bool hasCustomUI;
        bool canHasPlusButton;
    };

    static PluginUIAdapterRegistry& getInstance();

    // Register adapter for specific plugin type by type_index
    void registerAdapter(std::type_index pluginType, const AdapterInfo& adapter);

    // Register adapter for specific plugin type by type (template convenience method)
    // template<typename PluginType>
    // void registerAdapter(const AdapterInfo& adapter) {
    //     registerAdapter(std::type_index(typeid(PluginType)), adapter);
    // }

    template<typename PluginType, typename UIType>
    void registerAdapter(bool hasCustomUI, bool canHasPlusButton) {
        registerAdapter(std::type_index(typeid(PluginType)), {
            [](EditViewState& evs, tracktion::Plugin::Ptr plugin) -> std::unique_ptr<PluginDeviceUI> {
                return std::make_unique<UIType>(evs, plugin);
            },
            hasCustomUI,
            canHasPlusButton
        });
    }

    // Create device UI for plugin (returns nullptr if no adapter registered)
    std::unique_ptr<PluginDeviceUI> createDeviceUI(EditViewState& evs, tracktion::Plugin::Ptr plugin) const;

    // Check if plugin has custom device UI
    bool hasCustomDeviceUI(tracktion::Plugin::Ptr plugin) const;

    // Check if plugin can have plus button after it
    bool canHasPlusButtonAfter(tracktion::Plugin::Ptr plugin) const;

private:

    const PluginUIAdapterRegistry::AdapterInfo* findAdapterInfo(tracktion::Plugin::Ptr plugin) const;

    PluginUIAdapterRegistry() = default;
    std::unordered_map<std::type_index, AdapterInfo> adapters_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginUIAdapterRegistry)
};

/**
 * RAII helper for registering plugin UI adapters.
 * Use this in plugin implementation files to auto-register adapters.
 */
template<typename PluginType, typename UIType>
class PluginUIAdapterRegistrar {
public:
    PluginUIAdapterRegistrar(bool hasCustomUI, bool canHasPlusButton) {
        PluginUIAdapterRegistry::getInstance().registerAdapter<PluginType, UIType>(hasCustomUI, canHasPlusButton);
    }
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginUIAdapterRegistrar)
};

// Macro to make registration more convenient
#define REGISTER_PLUGIN_UI_ADAPTER(PluginType, UIType, hasCustomUI, canHasPlusButton) \
    namespace { \
        static const PluginUIAdapterRegistrar<PluginType, UIType> plugin_ui_adapter_registrar(hasCustomUI, canHasPlusButton); \
    }

// // Macro to make registration more convenient
// #define REGISTER_PLUGIN_UI_ADAPTER(PluginType, factory, hasCustomUI, canHasPlusButton) \
//     static const PluginUIAdapterRegistrar<PluginType> __plugin_ui_adapter_registrar_##PluginType({ \
//         factory, hasCustomUI, canHasPlusButton \
//     });

}  // namespace MoTool