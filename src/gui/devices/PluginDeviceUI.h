#pragma once

#include <JuceHeader.h>
#include <memory>

namespace MoTool {

template <typename T>
struct ParameterValue;

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
    virtual bool hasDeviceMenu() const { return false; }
    virtual void populateDeviceMenu(juce::PopupMenu& /*menu*/) {}

protected:
    tracktion::Plugin::Ptr plugin;
    static void addDiscreteIntegerParameterMenu(juce::PopupMenu& parentMenu,
                                                ParameterValue<int>& parameter,
                                                const juce::String& title);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginDeviceUI)
};

}  // namespace MoTool
