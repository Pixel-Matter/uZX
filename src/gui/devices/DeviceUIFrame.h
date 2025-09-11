#pragma once

#include <JuceHeader.h>
#include "../../controllers/EditState.h"
#include "PluginDeviceUI.h"

namespace MoTool {

/**
 * DeviceUIFrame wraps a PluginDeviceUI to provide a title bar with plugin name,
 * bypass button, and menu button.
 */
class DeviceUIFrame : public Component
{
public:
    DeviceUIFrame(EditViewState& evs, tracktion::Plugin::Ptr pluginPtr, PluginDeviceUI& ui);
    ~DeviceUIFrame() override;

    // Get the plugin this UI represents
    tracktion::Plugin::Ptr getPlugin() const { return plugin; }

    void resized() override;
    void paint(juce::Graphics& g) override;

protected:
    EditViewState& editViewState;
    PluginDeviceUI& pluginUI;
    tracktion::Plugin::Ptr plugin;

private:
    static constexpr int titleBarHeight = 16;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeviceUIFrame)
};

}  // namespace MoTool