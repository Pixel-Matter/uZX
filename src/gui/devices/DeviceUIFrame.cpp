#include "DeviceUIFrame.h"
#include "../common/LookAndFeel.h"
#include "juce_graphics/juce_graphics.h"

namespace MoTool {

DeviceUIFrame::DeviceUIFrame(EditViewState& evs, tracktion::Plugin::Ptr pluginPtr, PluginDeviceUI& ui)
    : editViewState(evs)
    , pluginUI(ui)
    , plugin(pluginPtr)
{
    addAndMakeVisible(&pluginUI);

    // Size the frame to accommodate the plugin UI plus some padding for the frame
    static constexpr int frameWidth = 0;  // 2px border on each side

    setSize(pluginUI.getWidth() + frameWidth,
            pluginUI.getHeight() + titleBarHeight + frameWidth);
}

DeviceUIFrame::~DeviceUIFrame() = default;

void DeviceUIFrame::resized() {
    // static constexpr int frameWidth = 2;

    auto bounds = getLocalBounds();
    bounds.removeFromTop(titleBarHeight); // Reserve space for title bar
    // bounds = bounds.reduced(frameWidth); // Reserve space for border

    pluginUI.setBounds(bounds);
}

void DeviceUIFrame::paint(Graphics& g) {
    // Draw frame background
    // g.fillAll(Colors::Theme::surface);

    // Draw title bar
    auto titleBar = getLocalBounds().removeFromTop(titleBarHeight);
    g.setColour(Colors::Theme::surface);
    g.fillRect(titleBar);

    // Draw plugin name in title bar
    if (plugin) {
        g.setColour(Colors::Theme::textPrimary);
        // font height always should be defined in points
        auto font = getLookAndFeel().getPopupMenuFont();
        g.setFont(font.withPointHeight(11.0f).withExtraKerningFactor(0.03f).withStyle(Font::bold));
        g.drawSingleLineText(
            plugin->getName(),
            4, titleBar.getHeight() - 4
        );
    }
}

}  // namespace MoTool