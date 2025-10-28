#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>
#include "../../controllers/EditState.h"
#include "../common/LookAndFeel.h"
#include "juce_gui_basics/juce_gui_basics.h"

namespace MoTool {

// Forward declaration
class PluginDeviceUI;

//==============================================================================
/**
 * Base class for all device panel items.
 * Provides a common base for both devices and buttons in the panel.
 */
class DevicePanelItemBase : public Component {
public:
    using Component::Component;
};

//==============================================================================
/**
 * Base class for all devices in devices panel
 * Provides a common interface for both framed and frameless device UIs.
 */
class DeviceItem : public DevicePanelItemBase {
public:
    DeviceItem(std::unique_ptr<PluginDeviceUI> ui);

    tracktion::Plugin::Ptr getPlugin() const { return plugin_; }

    // Pure virtual method to access the underlying device UI for plus button logic
    PluginDeviceUI* getDeviceUI() const { return ui_.get(); }

protected:
    tracktion::Plugin::Ptr plugin_;
    std::unique_ptr<PluginDeviceUI> ui_;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeviceItem)
};

//==============================================================================
/**
 * Frameless wrapper for custom device UIs (like LevelMeter).
 * Directly displays the PluginDeviceUI without additional framing.
 */
class FramelessDeviceItem : public DeviceItem {
public:
    FramelessDeviceItem(std::unique_ptr<PluginDeviceUI> ui);

    // Component overrides
    void resized() override;
    void paint(Graphics& g) override;

private:

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FramelessDeviceItem)
};


//==============================================================================
/**
 * BackgroundlessButton - Button that draws only text, no background
 */
class BackgroundlessTextButton : public TextButton {
public:
    BackgroundlessTextButton();
    explicit BackgroundlessTextButton(const String& buttonText);
    void paintButton(Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    virtual Colour getHighlightedTextColor(Colour textColor) const;

protected:
    virtual void setupColors();
};

//==============================================================================
/**
 * TitleBar - Internal component for handling title bar functionality
 */
class TitleBar : public Component {
public:
    TitleBar(tracktion::Plugin::Ptr plugin, PluginDeviceUI* deviceUI);

    void setCollapseToggle(std::function<void()> handler);
    void setCollapsed(bool collapsed);
    [[nodiscard]] bool isCollapsed() const { return isCollapsed_; }

    void paint(Graphics& g) override;
    void resized() override;
    void mouseUp(const MouseEvent& event) override;
    void mouseDoubleClick(const MouseEvent& event) override;

    static constexpr int height = 16;

private:
    void showDeviceMenu(juce::Component* target);
    void refreshMenuButtonState();
    tracktion::Plugin::Ptr plugin_;
    PluginDeviceUI* deviceUI_ = nullptr;
    BackgroundlessTextButton enableButton_;
    BackgroundlessTextButton menuButton_;
    std::function<void()> toggleCollapsed_;

    bool isCollapsed_ = false;

    static constexpr int titleBarHeight = height;
    static constexpr int buttonMargin = 0;
    static constexpr int buttonWidth = titleBarHeight - buttonMargin * 2;
};

//==============================================================================
/**
 * Framed wrapper for regular device UIs.
 * Provides a title bar with plugin name and wraps the PluginDeviceUI directly.
 */
class FramedDeviceItem : public DeviceItem {
public:
    FramedDeviceItem(std::unique_ptr<PluginDeviceUI> ui);

    // Component overrides
    void resized() override;
    void paint(Graphics& g) override;

    void setCollapsed(bool collapsed);
    [[nodiscard]] bool isCollapsed() const { return isCollapsed_; }

    static constexpr float cornerSize = 4.0f;
private:
    bool isCollapsed_ = false;
    int expandedWidth_ = 0;
    int expandedHeight_ = 0;

    TitleBar titleBar_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FramedDeviceItem)
};

}  // namespace MoTool
