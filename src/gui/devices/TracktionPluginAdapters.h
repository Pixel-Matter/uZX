#pragma once

#include "PluginUIAdapterRegistry.h"
#include "PluginDeviceUI.h"

namespace MoTool {

/**
 * Generic UI Adapter for tracktion plugins that don't have specific custom UIs.
 * Shows plugin name and provides click-to-open-editor functionality.
 */
class GenericTracktionPluginUIAdapter : public PluginDeviceUI {
public:
    GenericTracktionPluginUIAdapter(EditViewState& evs, tracktion::Plugin::Ptr pluginPtr);
    ~GenericTracktionPluginUIAdapter() override = default;

    // Component overrides
    void resized() override;
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    juce::String pluginName_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GenericTracktionPluginUIAdapter)
};

/**
 * Factory functions for tracktion plugin UI creation
 */
namespace TracktionPluginUIFactory {

    // LevelMeterPlugin factories
    std::unique_ptr<PluginDeviceUI> createLevelMeterUI(EditViewState& evs, tracktion::Plugin::Ptr plugin);
    bool levelMeterHasCustomUI(tracktion::Plugin::Ptr plugin);
    bool levelMeterCanHasPlusButton(tracktion::Plugin::Ptr plugin);

    // Generic tracktion plugin factories
    std::unique_ptr<PluginDeviceUI> createGenericUI(EditViewState& evs, tracktion::Plugin::Ptr plugin);
    bool genericHasCustomUI(tracktion::Plugin::Ptr plugin);
    bool genericCanHasPlusButton(tracktion::Plugin::Ptr plugin);
}

}  // namespace MoTool