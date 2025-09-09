#pragma once

#include "PluginUIAdapterRegistry.h"
#include "PluginDeviceUI.h"

namespace MoTool {

/**
 * Generic UI Adapter for tracktion plugins that don't have specific custom UIs.
 * Shows plugin name and provides click-to-open-editor functionality.
 */
class GenericPluginUIAdapter : public PluginDeviceUI {
public:
    GenericPluginUIAdapter(EditViewState& evs, tracktion::Plugin::Ptr pluginPtr);
    ~GenericPluginUIAdapter() override = default;

    // Component overrides
    void resized() override;
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GenericPluginUIAdapter)
};

}  // namespace MoTool