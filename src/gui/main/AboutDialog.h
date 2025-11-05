#pragma once

#include <JuceHeader.h>

#include "../common/LookAndFeel.h"

namespace MoTool {

class MultiLineTextComponent : public juce::Component {
public:
    MultiLineTextComponent();

    void setText(const juce::String& text);
    void setFont(juce::Font font);
    void setColour(juce::Colour colour);
    void setJustification(juce::Justification justification);
    void setLineSpacing(float lineSpacing);
    void setPadding(juce::BorderSize<int> padding);
    int getPreferredHeight(int width);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void refreshLayout(int width);
    void invalidateLayout();

    juce::String text_;
    juce::Font font_ { juce::FontOptions(14.0f, juce::Font::plain) };
    juce::Colour colour_ { juce::Colours::white };
    juce::Justification justification_ { juce::Justification::centred };
    float lineSpacingFactor_ { 1.0f };
    juce::BorderSize<int> contentPadding_;
    juce::TextLayout layout_;
    int layoutWidth_ { -1 };
};

class AboutDialogComponent : public juce::Component {
public:
    AboutDialogComponent();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::Label titleLabel_;
    juce::Label companyLabel_;
    MultiLineTextComponent infoText_;
    juce::HyperlinkButton websiteLink_ { "pixelmatter.org", juce::URL("https://pixelmatter.org") };
    juce::Label copyrightLabel_;
    juce::TextButton closeButton_;
};

} // namespace MoTool
