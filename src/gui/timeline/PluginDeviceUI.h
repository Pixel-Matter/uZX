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
    PluginDeviceUI(tracktion::Plugin::Ptr plugin);
    // PluginDeviceUI(EditViewState& evs, tracktion::Plugin::Ptr plugin);
    ~PluginDeviceUI() override;

    // Get the plugin this UI represents
    tracktion::Plugin::Ptr getPlugin() const { return plugin; }

    // Component overrides
    // void mouseDown(const juce::MouseEvent& e) override;

    // // Factory method to create appropriate UI for a plugin
    // static std::unique_ptr<PluginDeviceUI> createForPlugin(EditViewState& evs, tracktion::Plugin::Ptr plugin);

    static bool hasCustomDeviceUI(tracktion::Plugin::Ptr plugin);
    static bool canHasPlusButtonAfter(tracktion::Plugin::Ptr plugin);

protected:
    // EditViewState& editViewState;  // for selection management
    tracktion::Plugin::Ptr plugin;

    // Called when plugin is clicked - can be overridden
    // virtual void pluginClicked(const juce::ModifierKeys& modifiers);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginDeviceUI)
};

/**
 * DeviceContainer wraps a PluginDeviceUI to provide a title bar with plugin name,
 * bypass button, and menu button.
 */
class DeviceUIFrame : public Component
{
public:
    DeviceUIFrame(EditViewState& evs, tracktion::Plugin::Ptr plugin, te::Plugin::EditorComponent& editor);
    ~DeviceUIFrame() override;

    // Get the plugin this UI represents
    tracktion::Plugin::Ptr getPlugin() const { return plugin; }

    void resized() override;
    void paint(juce::Graphics& g) override;

protected:
    EditViewState& editViewState;
    te::Plugin::EditorComponent& pluginEditor;
    tracktion::Plugin::Ptr plugin;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeviceUIFrame)
};

}  // namespace MoTool