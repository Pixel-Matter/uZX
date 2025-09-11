#pragma once

#include <JuceHeader.h>
#include <memory>
#include "../../controllers/EditState.h"

namespace MoTool {

/**
 * Base class for plugin device UI components.
 * Provides a common interface for specialized plugin visualizations in Track Device Panel.
 */
class PluginDeviceUI: public juce::Component
{
public:
    PluginDeviceUI(EditViewState& evs, tracktion::Plugin::Ptr plugin);
    ~PluginDeviceUI() override;

    // Get the plugin this UI represents
    tracktion::Plugin::Ptr getPlugin() const { return plugin; }

    // Component overrides
    // void mouseDown(const juce::MouseEvent& e) override;

    // These methods now delegate to the adapter registry for backward compatibility
    virtual bool hasCustomDeviceUI() { return false; }
    virtual bool canHasPlusButtonAfter() { return true; }

protected:
    // EditViewState& editViewState;  // for selection management
    tracktion::Plugin::Ptr plugin;

    // Called when plugin is clicked - can be overridden
    // virtual void pluginClicked(const juce::ModifierKeys& modifiers);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginDeviceUI)
};

}  // namespace MoTool