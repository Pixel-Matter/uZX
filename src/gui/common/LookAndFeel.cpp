#include "LookAndFeel.h"

namespace MoTool {

MoLookAndFeel::MoLookAndFeel() {
    using namespace Colors;

    // used for numeric readouts
    // setDefaultSansSerifTypefaceName("Iosevka Aile");  // Very-very good, crisp and squary, programmer's 0
    setDefaultSansSerifTypefaceName("Inter");  // Very good

    setUsingNativeAlertWindows(true);

    // Initialize ColourScheme with slate colors
    LookAndFeel_V4::ColourScheme cs(
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
    setColour(ResizableWindow::backgroundColourId, Theme::background);
    setColour(DocumentWindow::backgroundColourId, Theme::background);

    // Buttons
    setColour(TextButton::buttonColourId, Theme::surface);
    setColour(TextButton::buttonOnColourId, Theme::primary);
    setColour(TextButton::textColourOffId, Theme::textPrimary);
    setColour(TextButton::textColourOnId, Theme::background);

    // Text editors
    setColour(TextEditor::backgroundColourId, Colours::transparentBlack);
    // setColour(TextEditor::backgroundColourId, Theme::background);
    setColour(TextEditor::textColourId, Theme::primary);
    // setColour(TextEditor::textColourId, Theme::textPrimary);
    // setColour(TextEditor::highlightColourId, Theme::backgroundAlt);
    setColour(TextEditor::highlightColourId, Theme::primary);
    setColour(TextEditor::highlightedTextColourId, Theme::background);
    setColour(TextEditor::outlineColourId, Theme::border);
    setColour(TextEditor::focusedOutlineColourId, Theme::border);
    setColour(TextEditor::shadowColourId, Theme::background);

    // Text cursor
    setColour(CaretComponent::caretColourId, Theme::primary);

    // Sliders
    setColour(Slider::backgroundColourId, Theme::surfaceAlt);
    setColour(Slider::trackColourId, Theme::primary);
    setColour(Slider::thumbColourId, Theme::textPrimary);

    // ComboBox
    setColour(ComboBox::backgroundColourId, Theme::surface);
    setColour(ComboBox::textColourId, Theme::textPrimary);
    setColour(ComboBox::arrowColourId, Theme::primary);
    setColour(ComboBox::outlineColourId, Theme::border);
    setColour(ComboBox::focusedOutlineColourId, Theme::primary);

    // PopupMenu
    setColour(PopupMenu::backgroundColourId, Theme::backgroundAlt);
    setColour(PopupMenu::textColourId, Theme::textPrimary);
    setColour(PopupMenu::highlightedBackgroundColourId, Theme::primary);
    setColour(PopupMenu::highlightedTextColourId, Theme::background);

    // TooltipWindow
    setColour(TooltipWindow::backgroundColourId, Theme::backgroundAlt);
    setColour(TooltipWindow::textColourId, Theme::textPrimary);
    setColour(TooltipWindow::outlineColourId, Theme::border);
}

void MoLookAndFeel::debugColourScheme() {
    const auto& cs = getCurrentColourScheme();
    ignoreUnused(cs);
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

void MoLookAndFeel::drawButtonBackground(Graphics& g, Button& button,
                        const Colour& backgroundColour,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) {
    // auto cornerSize = 6.0f;
    auto cornerSize = 4.0f;
    // auto bounds = button.getLocalBounds().toFloat().reduced(0.5f, 0.5f);
    auto bounds = button.getLocalBounds().toFloat();

    // auto baseColour = backgroundColour.withMultipliedSaturation(button.hasKeyboardFocus(true) ? 1.3f : 0.9f)
    //                                   .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f);

    // if (shouldDrawButtonAsDown || shouldDrawButtonAsHighlighted)
    //     baseColour = baseColour.contrasting(shouldDrawButtonAsDown ? 0.2f : 0.05f);
    // g.setColour(baseColour);
    g.setColour(shouldDrawButtonAsDown ? backgroundColour.darker(0.2f) :
                shouldDrawButtonAsHighlighted ? backgroundColour.brighter(0.1f) :
                backgroundColour);

    auto flatOnLeft = button.isConnectedOnLeft();
    auto flatOnRight = button.isConnectedOnRight();
    auto flatOnTop = button.isConnectedOnTop();
    auto flatOnBottom = button.isConnectedOnBottom();

    if (flatOnLeft) {
        bounds.removeFromLeft(1);
    }
    if (flatOnRight) {
        bounds.removeFromRight(1);
    }
    if (flatOnTop) {
        bounds.removeFromTop(1);
    }
    if (flatOnBottom) {
        bounds.removeFromBottom(1);
    }

    if (flatOnLeft || flatOnRight || flatOnTop || flatOnBottom) {
        Path path;
        path.addRoundedRectangle(bounds.getX(),
                                 bounds.getY(),
                                 bounds.getWidth(),
                                 bounds.getHeight(),
                                 cornerSize,
                                 cornerSize,
                                 !(flatOnLeft || flatOnTop),
                                 !(flatOnRight || flatOnTop),
                                 !(flatOnLeft || flatOnBottom),
                                 !(flatOnRight || flatOnBottom));

        g.fillPath(path);

        // g.setColour(button.findColour(ComboBox::outlineColourId));
        // g.strokePath(path, PathStrokeType(1.0f));
    } else {
        g.fillRoundedRectangle(bounds, cornerSize);

        // g.setColour(button.findColour(ComboBox::outlineColourId));
        // g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
    }
}

//==============================================================================
Font MoLookAndFeel::getTextButtonFont(TextButton&, int buttonHeight) {
    return withDefaultMetrics(
        FontOptions().withPointHeight(jmin(16.0f, (float) roundToInt((float) buttonHeight * 0.6f)))
    );
}

void MoLookAndFeel::drawButtonText(Graphics& g,
                    TextButton& button,
                    bool /*shouldDrawButtonAsHighlighted*/,
                    bool /*shouldDrawButtonAsDown*/) {
    Font font(getTextButtonFont(button, button.getHeight()));
    g.setFont(font);
    g.setColour(
        button.findColour(button.getToggleState() ? TextButton::textColourOnId : TextButton::textColourOffId)
            .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f)
    );

    // const int yIndent = jmin(2, button.proportionOfHeight(0.3f));
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
        //            Justification::centred, false);

        g.drawFittedText(
            button.getButtonText(),
            leftIndent, yIndent,
            textWidth, button.getHeight() - yIndent * 2,
            Justification::centred, 2
        );
    }
}

void MoLookAndFeel::drawClip(Graphics& g, const Rectangle<int>& bounds,
             bool isSelected, const Colour& clipColor) {
    auto cornerSize = 3.0f;
    g.setColour(isSelected ? Colors::Timeline::clipSelected : clipColor);
    g.fillRoundedRectangle(bounds.toFloat(), cornerSize);

    // Draw clip border
    g.setColour(isSelected ? Colors::Timeline::clipSelected.brighter() : clipColor.brighter());
    g.drawRoundedRectangle(bounds.toFloat(), cornerSize, 1.0f);
}

void MoLookAndFeel::drawRotarySlider(Graphics& g,
                                     int x, int y, int width, int height, float sliderPos,
                                     const float rotaryStartAngle, const float rotaryEndAngle,
                                     Slider& slider) {
    auto outline = slider.findColour(Slider::rotarySliderOutlineColourId);
    auto fill = slider.findColour(Slider::rotarySliderFillColourId);

    auto bounds = Rectangle<int>(x, y, width, height).toFloat().reduced(0);

    auto radius = jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    auto lineW = jmax(2.0f, radius * 0.15f);
    auto arcRadius = radius - lineW * 0.5f;

    Path backgroundArc;
    backgroundArc.addCentredArc(
        bounds.getCentreX(), bounds.getCentreY(), arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);

    g.setColour(outline);
    g.strokePath(backgroundArc, PathStrokeType(lineW, PathStrokeType::curved, PathStrokeType::rounded));

    // knob body
    // TODO decoration?
    auto bodyRadius = arcRadius - lineW * 1.5f;
    g.setColour(Colors::Theme::surfaceElevated);
    g.fillEllipse(bounds.getCentreX() - bodyRadius, bounds.getCentreY() - bodyRadius,
                  bodyRadius * 2.0f, bodyRadius * 2.0f);

    // knob tick line
    auto thumbLineLength = bodyRadius;
    // auto thumbLineLength = arcRadius;
    Point<float> centre(bounds.getCentreX(), bounds.getCentreY());
    Point<float> thumbEnd(centre.x + thumbLineLength * std::cos(toAngle - MathConstants<float>::halfPi),
                         centre.y + thumbLineLength * std::sin(toAngle - MathConstants<float>::halfPi));

    g.setColour(slider.findColour(Slider::thumbColourId));
    g.drawLine(centre.x, centre.y, thumbEnd.x, thumbEnd.y, lineW * 0.75f);

    // arc indicating current value
    if (slider.isEnabled()) {
        Path valueArc;
        valueArc.addCentredArc(
            bounds.getCentreX(), bounds.getCentreY(), arcRadius, arcRadius, 0.0f, rotaryStartAngle, toAngle, true);

        g.setColour(fill);
        g.strokePath(valueArc, PathStrokeType(lineW, PathStrokeType::curved, PathStrokeType::rounded));
    }

}

TextLayout MoLookAndFeel::layoutTooltipText(TypefaceMetricsKind metrics, const String& text, Colour colour) noexcept {
    const int maxToolTipWidth = 600;
    const float tooltipFontSize = 14.0f;

    AttributedString s;
    s.setJustification(Justification::left);
    s.setLineSpacing(tooltipFontSize * 0.2f); // 1.2 line spacing
    s.append(text, FontOptions(tooltipFontSize, Font::plain).withMetricsKind(metrics), colour);

    TextLayout tl;
    tl.createLayoutWithBalancedLineLengths(s, (float)maxToolTipWidth);
    return tl;
}

Rectangle<int> MoLookAndFeel::getTooltipBounds(const String& tipText, Point<int> screenPos, Rectangle<int> parentArea) {
    const TextLayout tl(layoutTooltipText(getDefaultMetricsKind(), tipText, Colours::black));

    auto w = (int) (tl.getWidth() + 14.0f);
    auto h = (int) (tl.getHeight() + 6.0f);

    return Rectangle<int>(screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                          screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6)  : screenPos.y + 6,
                          w, h)
            .constrainedWithin(parentArea);
}

void MoLookAndFeel::drawTooltip(Graphics& g, const String& text, int width, int height) {
    Rectangle<int> bounds(width, height);
    int hPad = 7, vPad = 3;
    auto cornerSize = 5.0f;

    g.setColour(findColour(TooltipWindow::backgroundColourId));
    g.fillRoundedRectangle(bounds.toFloat(), cornerSize);

    g.setColour(findColour(TooltipWindow::outlineColourId));
    g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f, 0.5f), cornerSize, 1.0f);

    auto tl = layoutTooltipText(
        getDefaultMetricsKind(), text, findColour(TooltipWindow::textColourId));

    tl.draw(g, {
        static_cast<float>(hPad), static_cast<float>(vPad),
        static_cast<float>(width), static_cast<float>(height)
    });
}

void MoLookAndFeel::drawTimelineCursor(Graphics& g, float position, int height) {
    g.setColour(Colors::Timeline::cursor);
    g.drawVerticalLine((int)position, 0.0f, (float)height);
}

void MoLookAndFeel::drawTimelineGrid(Graphics& g, const Rectangle<int>& bounds,
                      const std::vector<TimelineGridTick>& ticks) const {
    for (const auto& tick : ticks) {
        if (tick.x > bounds.getWidth())
            continue;
        g.setColour(Colors::Timeline::trackGridTickColors[tick.level]);
        g.drawVerticalLine(bounds.getX() + tick.x, (float)bounds.getY(), (float)bounds.getBottom());
    }
}

//==============================================================================
// Readout specific LookAndFeel subclass
//==============================================================================
ReadoutLookAndFeel::ReadoutLookAndFeel() = default;

Font ReadoutLookAndFeel::getNumericReadoutFont() const {
    return FontOptions("Iosevka Aile", "Semibold", 20.0f).withKerningFactor(-0.05f);
}

Font ReadoutLookAndFeel::getSliderPopupFont(Slider& /*slider*/) {
    return getNumericReadoutFont();
}

// TODO create different L&F mixin class for readout labels and sliders
void ReadoutLookAndFeel::setupReadoutLabel(Label& label) {
    label.setJustificationType(Justification::left);
    label.setKeyboardType(TextInputTarget::decimalKeyboard);
    label.setFont(getNumericReadoutFont());
    label.setEditable(true, true, false);  // editable on single click, double click, no loss of focus

    label.setColour(Label::textColourId, Colors::Theme::primary);
    label.setColour(Label::backgroundColourId, Colors::Theme::backgroundDark);
    label.setColour(Label::outlineColourId, Colors::Theme::border);

    label.setColour(TextEditor::textColourId, Colors::Theme::primary);
    label.setColour(TextEditor::backgroundColourId, Colors::Theme::backgroundDark);
    label.setColour(TextEditor::outlineColourId, Colors::Theme::border);
    label.setColour(TextEditor::highlightColourId, Colors::Theme::primary);
    label.setColour(TextEditor::highlightedTextColourId, Colors::Theme::background);
}

Label* ReadoutLookAndFeel::createSliderTextBox(Slider& slider) {
    auto* label = LookAndFeel_V4::createSliderTextBox(slider);
    setupReadoutLabel(*label);
    // label->setJustificationType(Justification::left);
    // label->setFont(getNumericReadoutFont());
    // label->setColour(Label::textColourId, Colors::Theme::primary);
    // label->setColour(Label::backgroundColourId, Colors::Theme::backgroundDark);
    // label->setColour(Label::outlineColourId, Colors::Theme::border);
    return label;
}

void ReadoutLookAndFeel::setupNumericTextEditor(TextEditor& editor) const {
    using namespace Colors;

    // TODO see how textbox is made in Slider and make it similar
    editor.setFont(getNumericReadoutFont());
    editor.setMultiLine(false);
    editor.setReturnKeyStartsNewLine(false);
    editor.setReadOnly(false);
    editor.setScrollbarsShown(false);
    editor.setCaretVisible(true);
    editor.setPopupMenuEnabled(true);
    editor.setSelectAllWhenFocused(true);
}

} // namespace MoTool