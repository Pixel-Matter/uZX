#pragma once

#include "PluginDeviceUI.h"
#include "PluginUIAdapterRegistry.h"

namespace MoTool {

/**
 * Generic UI Adapter for any tracktion plugin that doesn't have a specific adapter.
 * Shows plugin name and provides click-to-open-editor functionality.
 */
class GenericPluginUIAdapter : public PluginDeviceUI {
public:
    GenericPluginUIAdapter(tracktion::Plugin::Ptr pluginPtr);
    ~GenericPluginUIAdapter() override = default;

    // Component overrides
    void resized() override;
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    juce::String pluginName_;
    bool isExternal_ = false;
    bool hasEditor_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GenericPluginUIAdapter)
};

/**
 * Factory functions for generic plugin UI creation
 */
namespace GenericPluginUIFactory {
    std::unique_ptr<PluginDeviceUI> createGenericUI(tracktion::Plugin::Ptr plugin);
}

}  // namespace MoTool