#pragma once

#include <JuceHeader.h>

#include "../common/LookAndFeel.h"

namespace MoTool {

class AboutDialogComponent : public juce::Component {
public:
    AboutDialogComponent();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::Label titleLabel_;
    juce::Label companyLabel_;
    juce::TextEditor infoText_;
    juce::HyperlinkButton websiteLink_ { "pixelmatter.org", juce::URL("https://pixelmatter.org") };
    juce::Label copyrightLabel_;
    juce::TextButton closeButton_;
};

} // namespace MoTool
