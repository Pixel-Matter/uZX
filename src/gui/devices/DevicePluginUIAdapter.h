#pragma once

#include "PluginUIAdapterRegistry.h"
#include "PluginDeviceUI.h"
#include "../../plugins/uZX/DevicePlugin.h"

namespace MoTool {

/**
 * UI Adapter for builtin DevicePlugin-based plugins.
 * This wraps the DevicePlugin's createDeviceUI() in our PluginDeviceUI interface.
 */
class DevicePluginUIAdapter : public PluginDeviceUI {
public:
    DevicePluginUIAdapter(EditViewState& evs, tracktion::Plugin::Ptr pluginPtr);
    ~DevicePluginUIAdapter() override = default;

    // Component overrides
    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    std::unique_ptr<uZX::DevicePlugin::DevicePluginUI> deviceUI_;
    uZX::DevicePlugin* devicePlugin_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DevicePluginUIAdapter)
};

/**
 * Factory functions for DevicePlugin UI creation
 */
namespace DevicePluginUIFactory {
    std::unique_ptr<PluginDeviceUI> createUI(EditViewState& evs, tracktion::Plugin::Ptr plugin);
    bool hasCustomUI(tracktion::Plugin::Ptr plugin);
    bool canHasPlusButton(tracktion::Plugin::Ptr plugin);
}

}  // namespace MoTool