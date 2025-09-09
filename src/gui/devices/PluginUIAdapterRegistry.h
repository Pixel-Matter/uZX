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

    struct AdapterInfo {
        UIFactory factory;
    };

    static PluginUIAdapterRegistry& getInstance();

    // Register adapter for specific plugin type by type_index
    void registerAdapter(std::type_index pluginType, const AdapterInfo& adapter);

    template<typename PluginType, typename UIType>
    void registerAdapter() {
        registerAdapter(std::type_index(typeid(PluginType)), {
            [](EditViewState& evs, tracktion::Plugin::Ptr plugin) -> std::unique_ptr<PluginDeviceUI> {
                return std::make_unique<UIType>(evs, plugin);
            },
        });
    }

    // Create device UI for plugin (returns nullptr if no adapter registered)
    std::unique_ptr<PluginDeviceUI> createDeviceUI(EditViewState& evs, tracktion::Plugin::Ptr plugin) const;


private:

    const PluginUIAdapterRegistry::AdapterInfo* findAdapterInfo(const std::type_info& typeInfo) const;
    const PluginUIAdapterRegistry::AdapterInfo* findAdapterInfo(const tracktion::Plugin* plugin) const;

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
    PluginUIAdapterRegistrar() {
        PluginUIAdapterRegistry::getInstance().registerAdapter<PluginType, UIType>();
    }
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginUIAdapterRegistrar)
};

// Macro to make registration more convenient
#define REGISTER_PLUGIN_UI_ADAPTER(PluginType, UIType) \
    namespace { \
        static const PluginUIAdapterRegistrar<PluginType, UIType> plugin_ui_adapter_registrar {}; \
    }

}  // namespace MoTool