#include "DevicePluginUIAdapter.h"

namespace MoTool {

DevicePluginUIAdapter::DevicePluginUIAdapter(EditViewState& evs, tracktion::Plugin::Ptr pluginPtr)
    : PluginDeviceUI(evs, pluginPtr)
    , devicePlugin_(dynamic_cast<uZX::DevicePlugin*>(pluginPtr.get()))
{
    if (devicePlugin_) {
        deviceUI_ = devicePlugin_->createDeviceUI();
        if (deviceUI_) {
            addAndMakeVisible(deviceUI_.get());
            setSize(deviceUI_->getWidth(), deviceUI_->getHeight());
        }
    }
}

void DevicePluginUIAdapter::resized() {
    if (deviceUI_) {
        deviceUI_->setBounds(getLocalBounds());
    }
}

void DevicePluginUIAdapter::paint(juce::Graphics&) {
    // Let the wrapped device UI handle painting
    // Optional: add border or styling here if needed
}

// Auto-register the DevicePlugin adapter
REGISTER_PLUGIN_UI_ADAPTER(uZX::DevicePlugin, DevicePluginUIAdapter, true, true)

}  // namespace MoTool