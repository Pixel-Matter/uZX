#include "LookAndFeel.h"
#include "juce_gui_basics/juce_gui_basics.h"

namespace MoTool {

MoLookAndFeel::MoLookAndFeel() {
    using namespace Colors;

    // used for numeric readouts
    // setDefaultSansSerifTypefaceName("Iosevka Aile");  // Very-very good, crisp and squary, programmer's 0
    setDefaultSansSerifTypefaceName("Inter");  // Very good

    setUsingNativeAlertWindows(true);

    // Initialize ColourScheme with slate colors
    juce::LookAndFeel_V4::ColourScheme cs(
        Theme::background,     // windowBackground
        Theme::surface,        // widgetBackground
        Theme::backgroundAlt,  // menuBackground
        Theme::border,         // outline
        Theme::textPrimary,    // defaultText
        Theme::primary,        // defaultFill
        Theme::textPrimary,    // highlightedText
        Theme::primary,        // highlightedFill
        Theme::textPrimary     // menuText
    );

    setColourScheme(cs);

    // Set component-specific colors
    setColour(juce::ResizableWindow::backgroundColourId, Theme::background);
    setColour(juce::DocumentWindow::backgroundColourId, Theme::background);

    // Buttons
    setColour(juce::TextButton::buttonColourId, Theme::surface);
    setColour(juce::TextButton::buttonOnColourId, Theme::primary);
    setColour(juce::TextButton::textColourOffId, Theme::textPrimary);
    setColour(juce::TextButton::textColourOnId, Theme::background);

    // Text editors
    setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    // setColour(juce::TextEditor::backgroundColourId, Theme::background);
    setColour(juce::TextEditor::textColourId, Theme::primary);
    // setColour(juce::TextEditor::textColourId, Theme::textPrimary);
    // setColour(juce::TextEditor::highlightColourId, Theme::backgroundAlt);
    setColour(juce::TextEditor::highlightColourId, Theme::primary);
    setColour(juce::TextEditor::highlightedTextColourId, Theme::background);
    setColour(juce::TextEditor::outlineColourId, Theme::border);
    setColour(juce::TextEditor::focusedOutlineColourId, Theme::border);
    setColour(juce::TextEditor::shadowColourId, Theme::background);

    // Text cursor
    setColour(juce::CaretComponent::caretColourId, Theme::primary);

    // Sliders
    setColour(juce::Slider::backgroundColourId, Theme::surfaceAlt);
    setColour(juce::Slider::trackColourId, Theme::primary);
    setColour(juce::Slider::thumbColourId, Theme::textPrimary);

    // ComboBox
    setColour(juce::ComboBox::backgroundColourId, Theme::surface);
    setColour(juce::ComboBox::textColourId, Theme::textPrimary);
    setColour(juce::ComboBox::arrowColourId, Theme::primary);
    setColour(juce::ComboBox::outlineColourId, Theme::border);
    setColour(juce::ComboBox::focusedOutlineColourId, Theme::primary);

    // PopupMenu
    setColour(juce::PopupMenu::backgroundColourId, Theme::backgroundAlt);
    setColour(juce::PopupMenu::textColourId, Theme::textPrimary);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, Theme::primary);
    setColour(juce::PopupMenu::highlightedTextColourId, Theme::background);

    // TooltipWindow
    setColour(juce::TooltipWindow::backgroundColourId, Theme::backgroundAlt);
    setColour(juce::TooltipWindow::textColourId, Theme::textPrimary);
    setColour(juce::TooltipWindow::outlineColourId, Theme::border);
}

void MoLookAndFeel::debugColourScheme() {
    const auto& cs = getCurrentColourScheme();
    DBG("Current Colour Scheme:");
    DBG("windowBackground: " + cs.getUIColour(ColourScheme::windowBackground).toString());
    DBG("widgetBackground: " + cs.getUIColour(ColourScheme::widgetBackground).toString());
    DBG("menuBackground: " + cs.getUIColour(ColourScheme::menuBackground).toString());
    DBG("outline: " + cs.getUIColour(ColourScheme::outline).toString());
    DBG("defaultText: " + cs.getUIColour(ColourScheme::defaultText).toString());
    DBG("defaultFill: " + cs.getUIColour(ColourScheme::defaultFill).toString());
    DBG("highlightedText: " + cs.getUIColour(ColourScheme::highlightedText).toString());
    DBG("highlightedFill: " + cs.getUIColour(ColourScheme::highlightedFill).toString());
    DBG("menuText: " + cs.getUIColour(ColourScheme::menuText).toString());
}

void MoLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                        const juce::Colour& backgroundColour,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) {
    auto bounds = button.getLocalBounds().toFloat();
    auto cornerSize = 4.0f;

    g.setColour(shouldDrawButtonAsDown ? backgroundColour.darker(0.2f) :
                shouldDrawButtonAsHighlighted ? backgroundColour.brighter(0.1f) :
                backgroundColour);

    g.fillRoundedRectangle(bounds, cornerSize);
}

void MoLookAndFeel::drawButtonText(juce::Graphics& g,
                    juce::TextButton& button,
                    bool /*shouldDrawButtonAsHighlighted*/,
                    bool /*shouldDrawButtonAsDown*/) {
    juce::Font font(getTextButtonFont(button, button.getHeight()));
    g.setFont(font);
    g.setColour(
        button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId)
            .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f)
    );

    // const int yIndent = juce::jmin(2, button.proportionOfHeight(0.3f));
    const int yIndent = 1;

    // const int fontHeight = roundToInt(font.getHeight() * 0.6f);
    // const int leftIndent = jmin(fontHeight / 2, 2 + cornerSize / (button.isConnectedOnLeft() ? 4 : 2));
    // const int rightIndent = jmin(fontHeight / 2, 2 + cornerSize / (button.isConnectedOnRight() ? 4 : 2));
    const int leftIndent = 1;
    const int rightIndent = 1;
    const int textWidth = button.getWidth() - leftIndent - rightIndent;

    if (textWidth > 0) {
        // g.drawText(button.getButtonText(),
        //            leftIndent, yIndent,
        //            textWidth, button.getHeight() - yIndent * 2,
        //            juce::Justification::centred, false);

        g.drawFittedText(
            button.getButtonText(),
            leftIndent, yIndent,
            textWidth, button.getHeight() - yIndent * 2,
            juce::Justification::centred, 2
        );
    }
}

void MoLookAndFeel::drawClip(juce::Graphics& g, const juce::Rectangle<int>& bounds,
             bool isSelected, const juce::Colour& clipColor) {
    auto cornerSize = 3.0f;
    g.setColour(isSelected ? Colors::Timeline::clipSelected : clipColor);
    g.fillRoundedRectangle(bounds.toFloat(), cornerSize);

    // Draw clip border
    g.setColour(isSelected ? Colors::Timeline::clipSelected.brighter() : clipColor.brighter());
    g.drawRoundedRectangle(bounds.toFloat(), cornerSize, 1.0f);
}

juce::TextLayout MoLookAndFeel::layoutTooltipText(juce::TypefaceMetricsKind metrics, const juce::String& text, juce::Colour colour) noexcept {
    const int maxToolTipWidth = 600;
    const float tooltipFontSize = 14.0f;

    juce::AttributedString s;
    s.setJustification(juce::Justification::left);
    s.setLineSpacing(tooltipFontSize * 0.2f); // 1.2 line spacing
    s.append(text, juce::FontOptions(tooltipFontSize, juce::Font::plain).withMetricsKind(metrics), colour);

    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths(s, (float)maxToolTipWidth);
    return tl;
}

juce::Rectangle<int> MoLookAndFeel::getTooltipBounds(const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) {
    const juce::TextLayout tl(layoutTooltipText(getDefaultMetricsKind(), tipText, juce::Colours::black));

    auto w = (int) (tl.getWidth() + 14.0f);
    auto h = (int) (tl.getHeight() + 6.0f);

    return juce::Rectangle<int>(screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                          screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6)  : screenPos.y + 6,
                          w, h)
            .constrainedWithin(parentArea);
}

void MoLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height) {
    juce::Rectangle<int> bounds(width, height);
    int hPad = 7, vPad = 3;
    auto cornerSize = 5.0f;

    g.setColour(findColour(juce::TooltipWindow::backgroundColourId));
    g.fillRoundedRectangle(bounds.toFloat(), cornerSize);

    g.setColour(findColour(juce::TooltipWindow::outlineColourId));
    g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f, 0.5f), cornerSize, 1.0f);

    auto tl = layoutTooltipText(
        getDefaultMetricsKind(), text, findColour(juce::TooltipWindow::textColourId));

    tl.draw(g, {
        static_cast<float>(hPad), static_cast<float>(vPad),
        static_cast<float>(width), static_cast<float>(height)
    });
}

void MoLookAndFeel::drawTimelineCursor(juce::Graphics& g, float position, int height) {
    g.setColour(Colors::Timeline::cursor);
    g.drawVerticalLine((int)position, 0.0f, (float)height);
}

void MoLookAndFeel::drawTimelineGrid(juce::Graphics& g, const juce::Rectangle<int>& bounds,
                      const std::vector<TimelineGridTick>& ticks) const {
    for (const auto& tick : ticks) {
        if (tick.x > bounds.getWidth())
            continue;
        g.setColour(Colors::Timeline::trackGridTickColors[tick.level]);
        g.drawVerticalLine(bounds.getX() + tick.x, (float)bounds.getY(), (float)bounds.getBottom());
    }
}

juce::Font MoLookAndFeel::getNumericReadoutFont() const {
    return juce::FontOptions("Iosevka Aile", "Semibold", 20.0f).withKerningFactor(-0.05f);
}

juce::Font MoLookAndFeel::getSliderPopupFont(juce::Slider& /*slider*/) {
    return getNumericReadoutFont();
}

juce::Label* MoLookAndFeel::createSliderTextBox(juce::Slider& slider) {
    auto* label = juce::LookAndFeel_V4::createSliderTextBox(slider);
    label->setFont(getNumericReadoutFont());
    label->setColour(juce::Label::textColourId, Colors::Theme::primary);
    label->setJustificationType(juce::Justification::left);
    return label;
}

void MoLookAndFeel::setupNumericTextEditor(juce::TextEditor& editor) const {
    using namespace Colors;

    editor.setFont(getNumericReadoutFont());
    // TODO see how textbox is made in Slider
    editor.setMultiLine(false);
    editor.setReturnKeyStartsNewLine(false);
    editor.setReadOnly(false);
    editor.setScrollbarsShown(false);
    editor.setCaretVisible(true);
    editor.setPopupMenuEnabled(true);
    editor.setSelectAllWhenFocused(true);
}

} // namespace MoTool