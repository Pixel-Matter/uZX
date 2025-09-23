#include "PluginUIAdapterRegistry.h"
#include "PluginDeviceUI.h"

namespace MoTool {

PluginUIAdapterRegistry& PluginUIAdapterRegistry::getInstance() {
    static PluginUIAdapterRegistry instance;
    return instance;
}

void PluginUIAdapterRegistry::registerAdapter(std::type_index pluginType, const AdapterInfo& adapter) {
    adapters_[pluginType] = adapter;
}

const PluginUIAdapterRegistry::AdapterInfo* PluginUIAdapterRegistry::findAdapterInfo(const std::type_info& typeInfo) const {
    auto it = adapters_.find(std::type_index(typeInfo));
    if (it != adapters_.end() && it->second.factory) {
        return &it->second;
    }
    return nullptr;
}

const PluginUIAdapterRegistry::AdapterInfo* PluginUIAdapterRegistry::findAdapterInfo(const tracktion::Plugin* plugin) const {
    if (!plugin) return nullptr;
    return findAdapterInfo(typeid(*plugin));
}

std::unique_ptr<PluginDeviceUI> PluginUIAdapterRegistry::createDeviceUI(tracktion::Plugin::Ptr plugin) const {
    if (auto info = findAdapterInfo(plugin.get()); info != nullptr) {
        jassert(info->factory != nullptr);
        return info->factory(plugin);
    }
    return nullptr;
}

}  // namespace MoTool