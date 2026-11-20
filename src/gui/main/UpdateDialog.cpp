#include "UpdateDialog.h"

#include "../common/LookAndFeel.h"

using namespace juce;

namespace MoTool {

UpdateDialogComponent::UpdateDialogComponent(const String& appName,
                                             const String& tagPrefix,
                                             const String& currentVersion)
    : appName_(appName), tagPrefix_(tagPrefix), currentVersion_(currentVersion),
      request_([this](const var& releases) { showAnswer(releases); }) {
    setOpaque(true);

    message_.setText("Checking for updates...", dontSendNotification);
    message_.setFont(Fonts::ui(Fonts::body));
    message_.setJustificationType(Justification::centred);
    message_.setMinimumHorizontalScale(1.0f);  // a long line wraps rather than narrows
    message_.setColour(Label::textColourId, Colors::Theme::textPrimary);
    addAndMakeVisible(message_);

    link_.setColour(HyperlinkButton::textColourId, Colors::Theme::primary);
    addChildComponent(link_);

    closeButton_.setButtonText("Close");
    MoLookAndFeel::setLargeText(closeButton_);
    closeButton_.onClick = [this] {
        if (auto* window = findParentComponentOfClass<DialogWindow>())
            window->exitModalState(0);
    };
    closeButton_.setColour(TextButton::buttonColourId, Colors::Theme::primary);
    closeButton_.setColour(TextButton::textColourOffId, Colors::Theme::background);
    addAndMakeVisible(closeButton_);

    setSize(420, 156);
}

void UpdateDialogComponent::showAnswer(const var& releases) {
    if (const auto newer = UpdateCheck::findNewerRelease(releases, tagPrefix_, currentVersion_)) {
        message_.setText(appName_ + " " + newer->version + " is available.\nYou have " + currentVersion_ + ".",
                         dontSendNotification);
        showLink("Release notes and downloads", newer->page);
        return;
    }

    message_.setText(releases.isVoid() ? String("Couldn't reach GitHub to check for updates.")
                                       : appName_ + " " + currentVersion_ + " is the latest version.",
                     dontSendNotification);
    showLink("All releases on GitHub", UpdateCheck::getReleasesPage());
}

void UpdateDialogComponent::showLink(const String& text, const URL& url) {
    link_.setButtonText(text);
    link_.setURL(url);
    link_.setTooltip(url.toString(false));
    link_.setVisible(true);
    resized();
}

void UpdateDialogComponent::paint(Graphics& g) {
    g.fillAll(Colors::Theme::backgroundAlt);
}

void UpdateDialogComponent::resized() {
    auto bounds = getLocalBounds().reduced(24, 16);
    closeButton_.setBounds(bounds.removeFromBottom(40).withSizeKeepingCentre(120, 32));
    bounds.removeFromBottom(12);
    if (link_.isVisible())
        link_.setBounds(bounds.removeFromBottom(24));
    message_.setBounds(bounds);  // without a link, the message is centred in the link's space too
}

}  // namespace MoTool
