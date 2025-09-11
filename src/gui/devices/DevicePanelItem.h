#pragma once

#include <JuceHeader.h>
#include <memory>
#include "DeviceUIFrame.h"

namespace MoTool {

/**
 * Base class for all device panel items.
 * Provides a common interface for both framed and frameless device UIs.
 */
class DevicePanelItem : public juce::Component {
public:
    DevicePanelItem(std::unique_ptr<PluginDeviceUI> ui);
    virtual ~DevicePanelItem() = default;

    tracktion::Plugin::Ptr getPlugin() const { return plugin_; }

    // Pure virtual method to access the underlying device UI for plus button logic
    PluginDeviceUI* getDeviceUI() const { return ui_.get(); }

protected:
    tracktion::Plugin::Ptr plugin_;
    std::unique_ptr<PluginDeviceUI> ui_;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DevicePanelItem)
};

/**
 * Frameless wrapper for custom device UIs (like LevelMeter).
 * Directly displays the PluginDeviceUI without additional framing.
 */
class FramelessDeviceItem : public DevicePanelItem {
public:
    FramelessDeviceItem(std::unique_ptr<PluginDeviceUI> ui);

    // Component overrides
    void resized() override;
    void paint(juce::Graphics& g) override;

private:

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FramelessDeviceItem)
};

/**
 * Framed wrapper for regular device UIs.
 * Wraps the PluginDeviceUI with a DeviceUIFrame that provides title bar, etc.
 */
class FramedDeviceItem : public DevicePanelItem {
public:
    FramedDeviceItem(EditViewState& evs, std::unique_ptr<PluginDeviceUI> ui);

    // Component overrides
    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    std::unique_ptr<DeviceUIFrame> frame_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FramedDeviceItem)
};

}  // namespace MoTool