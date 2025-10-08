#include "AboutDialog.h"

#include <tracktion_engine/tracktion_engine.h>

using namespace juce;

namespace MoTool {

AboutDialogComponent::AboutDialogComponent() {
    setOpaque(true);

    companyLabel_.setText(String::fromUTF8(ProjectInfo::companyName), dontSendNotification);
    companyLabel_.setJustificationType(Justification::centred);
    companyLabel_.setColour(Label::textColourId, Colors::Theme::textPrimary);
    companyLabel_.setFont(FontOptions(16.0f, Font::plain));
    addAndMakeVisible(companyLabel_);

    websiteLink_.setTooltip("https://pixelmatter.org");
    websiteLink_.setColour(HyperlinkButton::textColourId, Colors::Theme::primary);
    addAndMakeVisible(websiteLink_);

    titleLabel_.setText(String::fromUTF8(ProjectInfo::projectName), dontSendNotification);
    titleLabel_.setFont(FontOptions(28.0f, Font::bold));
    titleLabel_.setJustificationType(Justification::centred);
    titleLabel_.setColour(Label::textColourId, Colors::Theme::textPrimary);
    addAndMakeVisible(titleLabel_);

    auto version = String("Version ") + String::fromUTF8(ProjectInfo::versionString);
    versionLabel_.setText(version, dontSendNotification);
    versionLabel_.setJustificationType(Justification::centred);
    versionLabel_.setColour(Label::textColourId, Colors::Theme::textSecondary);
    addAndMakeVisible(versionLabel_);

    juceVersionLabel_.setText(String("Built with JUCE ") + SystemStats::getJUCEVersion()
                              + " | Tracktion Engine " + tracktion::engine::Engine::getVersion(), dontSendNotification);
    juceVersionLabel_.setJustificationType(Justification::centred);
    juceVersionLabel_.setColour(Label::textColourId, Colors::Theme::textSecondary);
    addAndMakeVisible(juceVersionLabel_);

    auto year = String(Time::getCurrentTime().getYear());
    copyrightLabel_.setText(String::fromUTF8("© ") + year + " "
                            + String::fromUTF8(ProjectInfo::companyName), dontSendNotification);
    copyrightLabel_.setJustificationType(Justification::centred);
    copyrightLabel_.setColour(Label::textColourId, Colors::Theme::textSecondary);
    addAndMakeVisible(copyrightLabel_);

    closeButton_.setButtonText("Close");
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
    websiteLink_.setBounds(bounds.removeFromTop(24));

    bounds.removeFromTop(6);
    titleLabel_.setBounds(bounds.removeFromTop(48));

    bounds.removeFromTop(8);
    versionLabel_.setBounds(bounds.removeFromTop(24));

    bounds.removeFromTop(4);
    juceVersionLabel_.setBounds(bounds.removeFromTop(24));

    bounds.removeFromTop(12);
    auto buttonArea = bounds.removeFromBottom(44);
    bounds.removeFromBottom(12);
    auto copyrightArea = bounds.removeFromBottom(24);

    const auto buttonWidth = 120;
    const auto buttonHeight = 32;
    closeButton_.setBounds(Rectangle<int>(buttonWidth, buttonHeight)
                               .withCentre(buttonArea.getCentre()));
    copyrightLabel_.setBounds(copyrightArea);
}

} // namespace MoTool
