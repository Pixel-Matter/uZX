#include "AboutDialog.h"
#include "version.h"
#include <cmath>

using namespace juce;
namespace te = tracktion;

namespace MoTool {

MultiLineTextComponent::MultiLineTextComponent() {
    setInterceptsMouseClicks(false, false);
    setWantsKeyboardFocus(false);
    setOpaque(false);
}

void MultiLineTextComponent::setText(const String& text) {
    if (text_ == text) {
        return;
    }

    text_ = text;
    invalidateLayout();
    repaint();
}

void MultiLineTextComponent::setFont(Font font) {
    if (font_ == font) {
        return;
    }

    font_ = font;
    invalidateLayout();
    repaint();
}

void MultiLineTextComponent::setColour(Colour colour) {
    if (colour_ == colour) {
        return;
    }

    colour_ = colour;
    repaint();
}

void MultiLineTextComponent::setJustification(Justification justification) {
    if (justification_ == justification) {
        return;
    }

    justification_ = justification;
    invalidateLayout();
    repaint();
}

void MultiLineTextComponent::setLineSpacing(float lineSpacing) {
    if (lineSpacingFactor_ == lineSpacing) {
        return;
    }

    lineSpacingFactor_ = lineSpacing;
    invalidateLayout();
    repaint();
}

void MultiLineTextComponent::setPadding(BorderSize<int> padding) {
    if (contentPadding_ == padding) {
        return;
    }

    contentPadding_ = padding;
    invalidateLayout();
    repaint();
}

int MultiLineTextComponent::getPreferredHeight(int width) {
    refreshLayout(width);

    const auto contentHeight = layout_.getNumLines() > 0
        ? layout_.getHeight()
        : 0.0f;

    return contentPadding_.getTopAndBottom() + static_cast<int>(std::ceil(contentHeight));
}

void MultiLineTextComponent::paint(Graphics& g) {
    refreshLayout(getWidth());

    if (layout_.getNumLines() == 0) {
        return;
    }

    auto area = contentPadding_.subtractedFrom(getLocalBounds()).toFloat();
    layout_.draw(g, area);
}

void MultiLineTextComponent::resized() {
    refreshLayout(getWidth());
}

void MultiLineTextComponent::refreshLayout(int width) {
    if (width <= 0) {
        layout_ = TextLayout {};
        layoutWidth_ = width;
        return;
    }

    if (width == layoutWidth_) {
        return;
    }

    layoutWidth_ = width;

    const auto availableWidth = jmax(0, width - contentPadding_.getLeftAndRight());

    if (availableWidth <= 0 || text_.isEmpty()) {
        layout_ = TextLayout {};
        return;
    }

    AttributedString attributed;
    attributed.setJustification(justification_);
    const auto extraSpacing = lineSpacingFactor_ > 0.0f
        ? jmax(0.0f, (lineSpacingFactor_ - 1.0f) * font_.getHeight())
        : 0.0f;
    attributed.setLineSpacing(extraSpacing);
    attributed.append(text_, font_, colour_);

    layout_.createLayout(attributed, static_cast<float>(availableWidth));
}

void MultiLineTextComponent::invalidateLayout() {
    layoutWidth_ = -1;
    layout_ = TextLayout {};
}

AboutDialogComponent::AboutDialogComponent() {
    setOpaque(true);

    companyLabel_.setText(String::fromUTF8(ProjectInfo::companyName), dontSendNotification);
    companyLabel_.setJustificationType(Justification::centred);
    companyLabel_.setColour(Label::textColourId, Colors::Theme::textPrimary);
    companyLabel_.setFont(FontOptions(16.0f, Font::plain));
    addAndMakeVisible(companyLabel_);

    titleLabel_.setText(JUCEApplication::getInstance()->getApplicationName(), dontSendNotification);
    titleLabel_.setFont(FontOptions(48.0f, Font::bold));
    titleLabel_.setJustificationType(Justification::centred);
    titleLabel_.setColour(Label::textColourId, Colors::Theme::textPrimary);
    addAndMakeVisible(titleLabel_);

    websiteLink_.setTooltip("https://pixelmatter.org");
    websiteLink_.setColour(HyperlinkButton::textColourId, Colors::Theme::primary);
    addAndMakeVisible(websiteLink_);

    StringArray infoLines;
    const auto versionBase = String::fromUTF8(ProjectInfo::versionString);
    auto version = String("Version ") + versionBase;

    auto suffix = String::fromUTF8(MoTool::Build::versionSuffix);
    if (suffix.isNotEmpty()) {
        version += suffix;
    }
    infoLines.add(version);

    const auto buildTimestamp = String::fromUTF8(MoTool::Build::buildTimestamp);
    if (suffix.isNotEmpty() && buildTimestamp.isNotEmpty()) {
        infoLines.add("Built at " + buildTimestamp);
    }

    infoLines.add(String("Powered by ") + SystemStats::getJUCEVersion()
                  + " and " + te::Engine::getVersion());
    infoLines.add("Includes ayumi library by Peter Sovietov (true-grue)");
    infoLines.add("");
    infoLines.add("Greets to:");
    infoLines.add("diver, spke, n1k-o, bfox, wbcbz7, Pator, Megus");
    infoLines.add("and all ZX Spectrum musicians and demosceners!");

    infoText_.setFont(Font(FontOptions(14.0f, Font::plain)));
    infoText_.setColour(Colors::Theme::textPrimary);
    infoText_.setJustification(Justification::centred);
    infoText_.setLineSpacing(1.5f);
    infoText_.setPadding(BorderSize<int>(2, 0, 0, 0));
    auto infoContent = infoLines.joinIntoString("\n");
    infoText_.setText(infoContent);
    addAndMakeVisible(infoText_);

    auto year = String(Time::getCurrentTime().getYear());
    copyrightLabel_.setText(String::fromUTF8("© ") + year + " "
                            + String::fromUTF8(ProjectInfo::companyName), dontSendNotification);
    copyrightLabel_.setJustificationType(Justification::centred);
    copyrightLabel_.setColour(Label::textColourId, Colors::Theme::textSecondary);
    addAndMakeVisible(copyrightLabel_);

    closeButton_.setButtonText("Cool");
    closeButton_.onClick = [this] {
        if (auto* window = findParentComponentOfClass<DialogWindow>())
            window->exitModalState(0);
    };
    closeButton_.setColour(TextButton::buttonColourId, Colors::Theme::primary);
    closeButton_.setColour(TextButton::textColourOffId, Colors::Theme::background);
    addAndMakeVisible(closeButton_);

    setSize(420, 416);
}

void AboutDialogComponent::paint(Graphics& g) {
    g.fillAll(Colors::Theme::backgroundAlt);

    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(Colors::Theme::border);
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);
}

void AboutDialogComponent::resized() {
    auto bounds = getLocalBounds().reduced(24);

    companyLabel_.setBounds(bounds.removeFromTop(24));

    bounds.removeFromTop(4);
    titleLabel_.setBounds(bounds.removeFromTop(48));

    bounds.removeFromTop(4);
    websiteLink_.setBounds(bounds.removeFromTop(24));

    bounds.removeFromTop(24);
    auto infoHeight = infoText_.getPreferredHeight(bounds.getWidth());
    infoHeight = jmax(infoHeight, 0);
    infoText_.setBounds(bounds.removeFromTop(infoHeight));

    bounds.removeFromTop(12);
    auto buttonArea = bounds.removeFromBottom(44);
    bounds.removeFromBottom(12);
    auto copyrightArea = bounds.removeFromBottom(24);

    const auto buttonWidth = 120;
    const auto buttonHeight = 32;
    closeButton_.setBounds(Rectangle<int>(buttonWidth, buttonHeight).withCentre(buttonArea.getCentre()));
    copyrightLabel_.setBounds(copyrightArea);
}

} // namespace MoTool
