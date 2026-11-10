#include "AboutDialog.h"

#include <limits>
#include "../../version.h"
#include "controllers/App.h"
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

ScrollerComponent::ScrollerComponent(String text)
    : text_(std::move(text)) {
    setInterceptsMouseClicks(false, false);
}

void ScrollerComponent::advance(double timestampSec) {
    static constexpr double pixelsPerSecond = 50.0;
    if (startSec_ < 0.0)
        startSec_ = timestampSec;
    offset_ = (timestampSec - startSec_) * pixelsPerSecond;
    repaint();
}

void ScrollerComponent::paint(Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    const float loop = GlyphArrangement::getStringWidth(font_, text_) + bounds.getWidth() * 0.5f;
    g.setFont(font_);
    g.setColour(Colors::Theme::textPrimary);
    for (auto left = -(float) std::fmod(offset_, (double) loop); left < bounds.getRight(); left += loop)
        g.drawText(text_, bounds.withX(left).withWidth(loop), Justification::centredLeft, false);

    const auto background = Colors::Theme::backgroundAlt;  // the dialog's
    const float fade = 32.0f;
    g.setGradientFill({background, 0.0f, 0.0f, background.withAlpha(0.0f), fade, 0.0f, false});
    g.fillRect(bounds.withWidth(fade));
    g.setGradientFill({background.withAlpha(0.0f), bounds.getRight() - fade, 0.0f,
                       background, bounds.getRight(), 0.0f, false});
    g.fillRect(bounds.withTrimmedLeft(bounds.getWidth() - fade));
}

AboutDialogComponent::AboutDialogComponent()
    : greets_(String("Greets to diver, spke, n1k-o, bfox, wbcbz7, Pator, Megus, Volutar")
              + " and all ZX Spectrum musicians and demosceners!") {
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

    for (auto* link : {&websiteLink_, &githubLink_}) {
        link->setTooltip(link->getURL().toString(false));
        link->setColour(HyperlinkButton::textColourId, Colors::Theme::primary);
        addAndMakeVisible(*link);
    }

    StringArray infoLines;
    auto version = String("Version ") + MoToolApp::getTargetVersion();

    auto channelStr = String::fromUTF8(MoTool::Build::channel);
    if (channelStr.isNotEmpty()) {
        version += "-" + channelStr;
    }

    auto coreInfo = String("core ") + MoTool::Build::coreVersion;
    auto gitHash = String::fromUTF8(MoTool::Build::gitHash);
    if (channelStr.isNotEmpty() && gitHash.isNotEmpty()) {
        coreInfo += "+" + gitHash;
    }
    if (MoTool::Build::gitDirty) {
        coreInfo += "-dirty";
    }
    version += " (" + coreInfo + ")";

    infoLines.add(version);

    const auto buildTimestamp = String::fromUTF8(MoTool::Build::buildTimestamp);
    if (channelStr.isNotEmpty() && buildTimestamp.isNotEmpty()) {
        infoLines.add("Built at " + buildTimestamp);
    }

    infoLines.add(String("Powered by ") + SystemStats::getJUCEVersion()
                  + " and " + te::Engine::getVersion());
    infoLines.add("Includes ayumi library by Peter Sovietov (true-grue)");
    infoLines.add("Opens tracker modules with zxtune by Vitamin/CAIG");

    infoText_.setFont(Font(FontOptions(14.0f, Font::plain)));
    infoText_.setColour(Colors::Theme::textPrimary);
    infoText_.setJustification(Justification::centred);
    infoText_.setLineSpacing(1.5f);
    infoText_.setPadding(BorderSize<int>(2, 0, 0, 0));
    auto infoContent = infoLines.joinIntoString("\n");
    infoText_.setText(infoContent);
    addAndMakeVisible(infoText_);
    addAndMakeVisible(greets_);

    auto year = buildTimestamp.substring(0, 4);
    copyrightLabel_.setText(String::fromUTF8("© 2025–") + year + " "
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

    setSize(420, layOut(420));  // as tall as its content
}

void AboutDialogComponent::paint(Graphics& g) {
    g.fillAll(Colors::Theme::backgroundAlt);

    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(Colors::Theme::border);
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);
}

void AboutDialogComponent::resized() {
    layOut(getWidth());
}

int AboutDialogComponent::layOut(int width) {
    auto bounds = Rectangle<int>(width, std::numeric_limits<int>::max() / 2).reduced(24);

    companyLabel_.setBounds(bounds.removeFromTop(24));

    bounds.removeFromTop(4);
    titleLabel_.setBounds(bounds.removeFromTop(48));

    bounds.removeFromTop(4);
    websiteLink_.setBounds(bounds.removeFromTop(24));
    githubLink_.setBounds(bounds.removeFromTop(24));

    bounds.removeFromTop(24);
    infoText_.setBounds(bounds.removeFromTop(jmax(infoText_.getPreferredHeight(bounds.getWidth()), 0)));

    bounds.removeFromTop(12);
    greets_.setBounds(bounds.removeFromTop(24));

    bounds.removeFromTop(12);
    copyrightLabel_.setBounds(bounds.removeFromTop(24));

    bounds.removeFromTop(12);
    const auto buttonArea = bounds.removeFromTop(44);
    closeButton_.setBounds(Rectangle<int>(120, 32).withCentre(buttonArea.getCentre()));

    return buttonArea.getBottom() + 24;
}

} // namespace MoTool
