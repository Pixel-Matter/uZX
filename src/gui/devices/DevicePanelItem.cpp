#include "DevicePanelItem.h"
#include "PluginDeviceUI.h"
#include "DeviceUIFrame.h"

namespace MoTool {

//==============================================================================
// DevicePanelItem

DevicePanelItem::DevicePanelItem(std::unique_ptr<PluginDeviceUI> ui)
    : plugin_(ui->getPlugin())
    , ui_(std::move(ui))
{
}

//==============================================================================
// FramelessDeviceItem

FramelessDeviceItem::FramelessDeviceItem(std::unique_ptr<PluginDeviceUI> ui)
    : DevicePanelItem(std::move(ui))
{
    if (ui_) {
        addAndMakeVisible(ui_.get());
        setSize(ui_->getWidth(), ui_->getHeight());
    }
}

void FramelessDeviceItem::resized() {
    if (ui_) {
        ui_->setBounds(getLocalBounds());
    }
}

void FramelessDeviceItem::paint(juce::Graphics&) {
    // Frameless - let the UI handle all painting
}

//==============================================================================
// FramedDeviceItem

FramedDeviceItem::FramedDeviceItem(EditViewState& evs, std::unique_ptr<PluginDeviceUI> ui)
    : DevicePanelItem(std::move(ui))
{
    if (ui_) {
        frame_ = std::make_unique<DeviceUIFrame>(evs, plugin_, *ui_);
        addAndMakeVisible(frame_.get());
        setSize(frame_->getWidth(), frame_->getHeight());
    }
}

void FramedDeviceItem::resized() {
    if (frame_) {
        frame_->setBounds(getLocalBounds());
    }
}

void FramedDeviceItem::paint(juce::Graphics&) {
    // Frame handles all painting
}

}  // namespace MoTool