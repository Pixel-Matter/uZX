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

const PluginUIAdapterRegistry::AdapterInfo* PluginUIAdapterRegistry::findAdapterInfo(tracktion::Plugin::Ptr plugin) const {
    if (!plugin) return nullptr;

    auto it = adapters_.find(std::type_index(typeid(*plugin)));
    if (it != adapters_.end() && it->second.factory) {
        return &it->second;
    }

    return nullptr;
}

std::unique_ptr<PluginDeviceUI> PluginUIAdapterRegistry::createDeviceUI(EditViewState& evs, tracktion::Plugin::Ptr plugin) const {
    if (!plugin) return nullptr;

    if (auto info = findAdapterInfo(plugin); info != nullptr) {
        return info->factory(evs, plugin);
    }

    return nullptr;
}

bool PluginUIAdapterRegistry::hasCustomDeviceUI(tracktion::Plugin::Ptr plugin) const {
    if (!plugin) return false;

    auto it = adapters_.find(std::type_index(typeid(*plugin)));
    if (it != adapters_.end()) {
        return it->second.hasCustomUI;
    }

    return false;
}

bool PluginUIAdapterRegistry::canHasPlusButtonAfter(tracktion::Plugin::Ptr plugin) const {
    if (!plugin) return true;  // Default: allow plus button

    auto it = adapters_.find(std::type_index(typeid(*plugin)));
    if (it != adapters_.end()) {
        return it->second.canHasPlusButton;
    }

    return true;  // Default: allow plus button
}

}  // namespace MoTool