#pragma once

#include <JuceHeader.h>
#include <memory>

namespace MoTool {

/**
 * Base class for plugin device UI components.
 * Provides a common interface for specialized plugin visualizations in Track Device Panel.
 */
class PluginDeviceUI: public juce::Component
{
public:
    PluginDeviceUI(tracktion::Plugin::Ptr plugin);
    ~PluginDeviceUI() override;

    tracktion::Plugin::Ptr getPlugin() const { return plugin; }

    virtual bool hasCustomDeviceUI() { return false; }
    virtual bool canHasPlusButtonAfter() { return true; }

protected:
    tracktion::Plugin::Ptr plugin;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginDeviceUI)
};

}  // namespace MoTool