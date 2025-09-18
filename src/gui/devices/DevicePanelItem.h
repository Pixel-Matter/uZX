#pragma once

#include <JuceHeader.h>
#include <memory>
#include "../../controllers/EditState.h"
#include "../common/LookAndFeel.h"

namespace MoTool {

// Forward declaration
class PluginDeviceUI;

/**
 * Base class for all device panel items.
 * Provides a common interface for both framed and frameless device UIs.
 */
class DevicePanelItem : public Component {
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
    void paint(Graphics& g) override;

private:

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FramelessDeviceItem)
};


//==============================================================================
// BackgroundlessButton - Button that draws only text, no background
class BackgroundlessButton : public TextButton {
public:
    void paintButton(Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

//==============================================================================
// TitleBar - Internal component for handling title bar functionality
class TitleBar : public Component {
public:
    TitleBar(tracktion::Plugin::Ptr plugin);

    void paint(Graphics& g) override;
    void resized() override;
    void mouseUp(const MouseEvent& event) override;

private:
    tracktion::Plugin::Ptr plugin_;
    BackgroundlessButton enableButton_;
    static constexpr int titleBarHeight = 16;
    static constexpr int buttonMargin = 0;
    static constexpr int buttonWidth = titleBarHeight - buttonMargin * 2;
};

/**
 * Framed wrapper for regular device UIs.
 * Provides a title bar with plugin name and wraps the PluginDeviceUI directly.
 */
class FramedDeviceItem : public DevicePanelItem {
public:
    FramedDeviceItem(std::unique_ptr<PluginDeviceUI> ui);

    // Component overrides
    void resized() override;
    void paint(Graphics& g) override;

    static constexpr float cornerSize = 4.0f;
private:
    TitleBar titleBar_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FramedDeviceItem)
};

}  // namespace MoTool