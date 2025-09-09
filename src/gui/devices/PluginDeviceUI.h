#pragma once

#include <JuceHeader.h>
#include "../../controllers/EditState.h"

namespace MoTool {

/**
 * Base class for plugin device UI components.
 * Provides a common interface for specialized plugin visualizations.
 */
class PluginDeviceUI : public juce::Component
{
public:
    PluginDeviceUI(EditViewState& evs, tracktion::Plugin::Ptr plugin);
    virtual ~PluginDeviceUI() override;
    
    // Get the plugin this UI represents
    tracktion::Plugin::Ptr getPlugin() const { return plugin; }
    
    // Component overrides
    void mouseDown(const juce::MouseEvent& e) override;
    
    // Factory method to create appropriate UI for a plugin
    static std::unique_ptr<PluginDeviceUI> createForPlugin(EditViewState& evs, tracktion::Plugin::Ptr plugin);
    
    // Check if a plugin type should have a custom device UI
    static bool hasCustomDeviceUI(tracktion::Plugin::Ptr plugin);

protected:
    EditViewState& editViewState;
    tracktion::Plugin::Ptr plugin;
    
    // Called when plugin is clicked - can be overridden
    virtual void pluginClicked(const juce::ModifierKeys& modifiers);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginDeviceUI)
};

}  // namespace MoTool