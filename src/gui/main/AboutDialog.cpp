#include "AboutDialog.h"
#include "version.h"
#include <cmath>

using namespace juce;
namespace te = tracktion;

namespace MoTool {

AboutDialogComponent::AboutDialogComponent() {
    setOpaque(true);

    companyLabel_.setText(String::fromUTF8(ProjectInfo::companyName), dontSendNotification);
    companyLabel_.setJustificationType(Justification::centred);
    companyLabel_.setColour(Label::textColourId, Colors::Theme::textPrimary);
    companyLabel_.setFont(FontOptions(16.0f, Font::plain));
    addAndMakeVisible(companyLabel_);

    titleLabel_.setText(String::fromUTF8(ProjectInfo::projectName), dontSendNotification);
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

    infoText_.setReadOnly(true);
    infoText_.setMultiLine(true, true);
    infoText_.setScrollbarsShown(false);
    infoText_.setCaretVisible(false);
    infoText_.setPopupMenuEnabled(false);
    infoText_.setInterceptsMouseClicks(false, false);
    infoText_.setJustification(Justification::centred);
    infoText_.setOpaque(false);
    infoText_.setWantsKeyboardFocus(false);
    infoText_.setIndents(0, 2);
    infoText_.setBorder({});
    infoText_.setColour(TextEditor::textColourId, Colors::Theme::textSecondary);
    infoText_.setColour(TextEditor::backgroundColourId, Colours::transparentBlack);
    infoText_.setColour(TextEditor::outlineColourId, Colours::transparentBlack);
    infoText_.setColour(TextEditor::focusedOutlineColourId, Colours::transparentBlack);
    infoText_.setFont(Font(FontOptions(14.0f, Font::plain)));
    infoText_.setLineSpacing(1.5f);
    auto infoContent = infoLines.joinIntoString("\n");
    infoText_.setText(infoContent, false);
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

    setSize(420, 320);
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
    const auto infoLinesText = infoText_.getText();
    StringArray lines;
    lines.addLines(infoLinesText);
    auto infoLinesCount = jmax(1, lines.size());

    auto fontHeight = infoText_.getFont().getHeight();
    auto spacing = infoText_.getLineSpacing();
    auto contentHeight = static_cast<int>(std::ceil(fontHeight * spacing * infoLinesCount));
    contentHeight = jmax(contentHeight, (int) std::ceil(fontHeight));
    contentHeight += infoText_.getTopIndent();
    contentHeight += infoText_.getBorder().getTopAndBottom();
    infoText_.setBounds(bounds.removeFromTop(contentHeight));

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
