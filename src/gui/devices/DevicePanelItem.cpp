#include "DevicePanelItem.h"
#include "PluginDeviceUI.h"


namespace MoTool {

//==============================================================================
void BackgroundlessToggleButton::paintButton(Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) {
    // Only draw the text, no background
    Colour textColour;
    if (getToggleState()) {
        textColour = findColour(TextButton::buttonOnColourId);
    } else {
        textColour = Colors::Theme::textDisabled;
    }

    if (shouldDrawButtonAsHighlighted)
        textColour = textColour.brighter(0.4f);

    if (shouldDrawButtonAsDown)
        textColour = textColour.darker(0.3f);

    g.setColour(textColour);
    auto font = getLookAndFeel().withDefaultMetrics(FontOptions().withPointHeight(jmin(16.0f, (float) getHeight())));
    g.setFont(font);
    g.drawText(getButtonText(), getLocalBounds(), Justification::centred, true);
}

//==============================================================================
TitleBar::TitleBar(tracktion::Plugin::Ptr plugin)
    : plugin_(plugin)
{
    setSize(getWidth(), titleBarHeight);

    // Setup enable/disable button
    enableButton_.setButtonText(String::fromUTF8("⏻"));
    // enableButton_.setButtonText(String::fromUTF8("●"));
    enableButton_.setSize(buttonWidth, buttonWidth);
    enableButton_.setToggleable(true);
    enableButton_.setToggleState(plugin->isEnabled(), juce::dontSendNotification);
    enableButton_.onClick = [this]() {
        plugin_->setEnabled(!enableButton_.getToggleState());
        enableButton_.setToggleState(plugin_->isEnabled(), juce::dontSendNotification);
    };
    addAndMakeVisible(enableButton_);
}

void TitleBar::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    Path path;
    path.addRoundedRectangle(
        bounds.getX(), bounds.getY(),
        bounds.getWidth(), bounds.getHeight(),
        FramedDeviceItem::cornerSize, FramedDeviceItem::cornerSize,
        true, true, false, false
    );
    g.setColour(findColour(juce::TextButton::buttonColourId));
    g.fillPath(path);

    if (plugin_) {
        g.setColour(Colors::Theme::textPrimary);
        auto font = g.getCurrentFont();
        g.setFont(font.withPointHeight(11.0f).withExtraKerningFactor(0.03f).withStyle(juce::Font::bold));

        // Draw plugin name, accounting for button space
        int textX = buttonWidth + buttonMargin * 2 + 4;
        g.drawSingleLineText(
            plugin_->getName(),
            textX, getHeight() - 4
        );
    }
}

void TitleBar::resized() {
    enableButton_.setBounds(buttonMargin, buttonMargin, buttonWidth, getHeight() - buttonMargin * 2);
}

void TitleBar::mouseUp(const juce::MouseEvent& event) {
    if (event.mods.isPopupMenu()) {
        juce::PopupMenu m;
        m.addItem("Delete", [this] { plugin_->deleteFromParent(); });
        m.showAt(this);
    }
}

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

FramedDeviceItem::FramedDeviceItem(std::unique_ptr<PluginDeviceUI> ui)
    : DevicePanelItem(std::move(ui))
    , titleBar_(plugin_)
{
    if (ui_) {
        // Create and add title bar
        addAndMakeVisible(titleBar_);
        addAndMakeVisible(ui_.get());

        // Size the frame to accommodate the plugin UI plus title bar
        static constexpr int frameWidth = 0;  // 2px border on each side
        setSize(ui_->getWidth() + frameWidth,
                ui_->getHeight() + titleBar_.getHeight() + frameWidth);
    }
}

void FramedDeviceItem::resized() {
    auto bounds = getLocalBounds();
    auto titleBarBounds = bounds.removeFromTop(titleBar_.getHeight());
    titleBar_.setBounds(titleBarBounds);
    if (ui_) {
        ui_->setBounds(bounds);
    }
}

void FramedDeviceItem::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    bounds.removeFromTop((float) titleBar_.getHeight());
    Path path;
    path.addRoundedRectangle(
        bounds.getX(), bounds.getY(),
        bounds.getWidth(), bounds.getHeight(),
        cornerSize, cornerSize,
        false, false, true, true
    );
    g.setColour(Colors::Theme::backgroundAlt);
    g.fillPath(path);
}

}  // namespace MoTool