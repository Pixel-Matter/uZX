#include "LookAndFeel.h"
#include "BinaryData.h"

namespace MoTool {

MoLookAndFeel::MoLookAndFeel() {
    using namespace Colors;

    // Embedded fonts loaded from BinaryData
    interRegularTypeface_ = Typeface::createSystemTypefaceFor(
        BinaryData::InterRegular_ttf, BinaryData::InterRegular_ttfSize);
    interBoldTypeface_ = Typeface::createSystemTypefaceFor(
        BinaryData::InterBold_ttf, BinaryData::InterBold_ttfSize);
    iosevkaRegularTypeface_ = Typeface::createSystemTypefaceFor(
        BinaryData::IosevkaAileRegular_ttf, BinaryData::IosevkaAileRegular_ttfSize);
    iosevkaSemiBoldTypeface_ = Typeface::createSystemTypefaceFor(
        BinaryData::IosevkaAileSemiBold_ttf, BinaryData::IosevkaAileSemiBold_ttfSize);

    setDefaultSansSerifTypefaceName(interRegularTypeface_->getName());

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

    // GroupComponent
    setColour(GroupComponent::outlineColourId, Theme::border);
    setColour(GroupComponent::textColourId, Theme::textSecondary);
}

Typeface::Ptr MoLookAndFeel::getTypefaceForFont(const Font& font) {
    auto name = font.getTypefaceName();
    bool isDefault = (name == Font::getDefaultSansSerifFontName());
    bool isInter   = name.containsIgnoreCase("Inter");
    auto style = font.getTypefaceStyle();

    if (isInter || isDefault) {
        if (style.containsIgnoreCase("Bold"))
            return interBoldTypeface_;
        return interRegularTypeface_;
    }

    if (name.containsIgnoreCase("Iosevka Aile")) {
        if (style.containsIgnoreCase("Semibold") || style.containsIgnoreCase("Semi Bold"))
            return iosevkaSemiBoldTypeface_;
        return iosevkaRegularTypeface_;
    }

    auto typeface = LookAndFeel_V4::getTypefaceForFont(font);
    return typeface != nullptr ? typeface : interRegularTypeface_;
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
    auto baseColour = shouldDrawButtonAsDown ? backgroundColour.darker(0.2f) :
                      shouldDrawButtonAsHighlighted ? backgroundColour.brighter(0.1f) :
                      backgroundColour;

    // Apply transparency for disabled buttons
    if (!button.isEnabled()) {
        baseColour = baseColour.withAlpha(0.3f);
    }

    g.setColour(baseColour);

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
Font MoLookAndFeel::getSmallFont(float pointHeigth) {
    return withDefaultMetrics(FontOptions().withPointHeight(jmin(16.0f, pointHeigth)));
}

Font MoLookAndFeel::getComboBoxFont(ComboBox& box) {
    return withDefaultMetrics(FontOptions{jmin(16.0f, (float) box.getHeight() * 0.85f)});
}

Font MoLookAndFeel::getPopupMenuFont() {
    return withDefaultMetrics(FontOptions(17.0f));
}

float MoLookAndFeel::getPopupMenuItemFontProportion() {
    return 1.3f;
}

float MoLookAndFeel::getComboBoxPopupMenuItemHeight(Label& label) {
    auto fontSize = label.getFont().getHeight();
    fontSize = jmax(fontSize, 18.0f);
    return fontSize * getPopupMenuItemFontProportion();
}

PopupMenu::Options MoLookAndFeel::getOptionsForComboBoxPopupMenu(ComboBox& box, Label& label) {
    return LookAndFeel_V4::getOptionsForComboBoxPopupMenu(box, label)
        .withStandardItemHeight(getComboBoxPopupMenuItemHeight(label));
}

//==============================================================================
void MoLookAndFeel::drawPopupMenuItem(Graphics& g,
                                      const Rectangle<int>& area,
                                      const bool isSeparator,
                                      const bool isActive,
                                      const bool isHighlighted,
                                      const bool isTicked,
                                      const bool hasSubMenu,
                                      const String& text,
                                      const String& shortcutKeyText,
                                      const Drawable* icon,
                                      const Colour* const textColourToUse) {
    if (isSeparator) {
        auto r = area.reduced(5, 0);
        r.removeFromTop(roundToInt(((float) r.getHeight() * 0.5f) - 0.5f));

        g.setColour(findColour(PopupMenu::textColourId).withAlpha(0.3f));
        g.fillRect(r.removeFromTop(1));
    } else {
        auto textColour = (textColourToUse == nullptr ? findColour(PopupMenu::textColourId) : *textColourToUse);

        auto r = area.reduced(1);

        if (isHighlighted && isActive) {
            g.setColour(findColour(PopupMenu::highlightedBackgroundColourId));
            g.fillRect(r);

            g.setColour(findColour(PopupMenu::highlightedTextColourId));
        } else {
            g.setColour(textColour.withMultipliedAlpha(isActive ? 1.0f : 0.5f));
        }

        r.reduce(jmin(5, area.getWidth() / 20), 0);

        auto font = getPopupMenuFont();

        auto maxFontHeight = (float) r.getHeight() / getPopupMenuItemFontProportion();

        if (font.getHeight() > maxFontHeight)
            font.setHeight(maxFontHeight);

        g.setFont(font);

        auto iconArea = r.removeFromLeft(roundToInt(maxFontHeight)).toFloat();

        if (icon != nullptr) {
            icon->drawWithin(g, iconArea, RectanglePlacement::centred | RectanglePlacement::onlyReduceInSize, 1.0f);
            r.removeFromLeft(roundToInt(maxFontHeight * 0.5f));
        } else if (isTicked) {
            auto tick = getTickShape(1.0f);
            g.fillPath(tick,
                       tick.getTransformToScaleToFit(iconArea.reduced(iconArea.getWidth() / 5, 0).toFloat(), true));
        }

        if (hasSubMenu) {
            auto arrowH = 0.6f * getPopupMenuFont().getAscent();

            auto x = static_cast<float>(r.removeFromRight((int) arrowH).getX());
            auto halfH = static_cast<float>(r.getCentreY());

            Path path;
            path.startNewSubPath(x, halfH - arrowH * 0.5f);
            path.lineTo(x + arrowH * 0.6f, halfH);
            path.lineTo(x, halfH + arrowH * 0.5f);

            g.strokePath(path, PathStrokeType(2.0f));
        }

        r.removeFromRight(3);
        g.drawFittedText(text, r, Justification::centredLeft, 1);

        if (shortcutKeyText.isNotEmpty()) {
            auto f2 = font;
            f2.setHeight(f2.getHeight() * 0.75f);
            f2.setHorizontalScale(0.95f);
            g.setFont(f2);

            g.drawText(shortcutKeyText, r, Justification::centredRight, true);
        }
    }
}

void MoLookAndFeel::getIdealPopupMenuItemSize(const String& text, const bool isSeparator,
                                              int standardMenuItemHeight,
                                              int& idealWidth, int& idealHeight) {
    if (isSeparator) {
        idealWidth = 50;
        idealHeight = standardMenuItemHeight > 0 ? standardMenuItemHeight / 10 : 10;
    } else {
        const auto prop = getPopupMenuItemFontProportion();
        auto font = getPopupMenuFont();

        if (standardMenuItemHeight > 0 && font.getHeight() > (float) standardMenuItemHeight / prop)
            font.setHeight((float) standardMenuItemHeight / prop);

        idealHeight = standardMenuItemHeight > 0 ? standardMenuItemHeight : roundToInt(font.getHeight() * prop);
        idealWidth = GlyphArrangement::getStringWidthInt(font, text) + idealHeight * 2;
    }
}

Font MoLookAndFeel::getTextButtonFont(TextButton&, int buttonHeight) {
    const int capHeight = buttonHeight - 10;
    const float pointHeight = (float) capHeight / 0.75f; // approx cap height to point size
    return getSmallFont(pointHeight);
}

void MoLookAndFeel::drawButtonText(Graphics& g,
                    TextButton& button,
                    bool /*shouldDrawButtonAsHighlighted*/,
                    bool /*shouldDrawButtonAsDown*/) {
    Font font(getTextButtonFont(button, button.getHeight()));

    g.setFont(font);
    auto color = button.findColour(button.getToggleState() ? TextButton::textColourOnId : TextButton::textColourOffId)
                       .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.3f);
    g.setColour(color);
    // const int yIndent = jmin(2, button.proportionOfHeight(0.3f));
    const int xIndent = 1;
    const int yIndent = roundToInt(((float) button.getHeight() - font.getHeightInPoints() * 0.75) / 2.0f);

    auto textRect = button.getLocalBounds().reduced(xIndent, yIndent);

    if (button.getWidth() > xIndent * 2) {
        g.drawFittedText(button.getButtonText(), textRect, Justification::centred, 1);
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

int MoLookAndFeel::getSliderThumbRadius(Slider& slider) {
    return jmin(10, slider.isHorizontal() ? static_cast<int>((float) slider.getHeight() * 0.5f)
                                         : static_cast<int>((float) slider.getWidth() * 0.5f));
}

// void MoLookAndFeel::drawLinearSlider(Graphics& g, int x, int y, int width, int height,
//                                      float sliderPos, float minSliderPos, float maxSliderPos,
//                                      const Slider::SliderStyle style, Slider& slider) {

//     g.setColour(slider.findColour(Slider::trackColourId).withAlpha(0.2f));
//     g.fillRect(x, y, width, height); // clear background

//     if (slider.isBar()) {
//         g.setColour(slider.findColour(Slider::trackColourId));
//         g.fillRect(
//             slider.isHorizontal()
//                 ? Rectangle<float>(
//                       static_cast<float>(x), (float) y + 0.5f, sliderPos - (float) x, (float) height - 1.0f)
//                 : Rectangle<float>(
//                       (float) x + 0.5f, sliderPos, (float) width - 1.0f, (float) y + ((float) height - sliderPos)));

//         drawLinearSliderOutline(g, x, y, width, height, style, slider);
//     } else {
//         auto isTwoVal =
//             (style == Slider::SliderStyle::TwoValueVertical || style == Slider::SliderStyle::TwoValueHorizontal);
//         auto isThreeVal =
//             (style == Slider::SliderStyle::ThreeValueVertical || style == Slider::SliderStyle::ThreeValueHorizontal);

//         auto trackWidth = jmin(4.0f, slider.isHorizontal() ? (float) height * 0.2f : (float) width * 0.2f);

//         Point<float> startPoint(slider.isHorizontal() ? (float) x : (float) x + (float) width * 0.5f,
//                                 slider.isHorizontal() ? (float) y + (float) height * 0.5f : (float) (height + y));

//         Point<float> endPoint(slider.isHorizontal() ? (float) (width + x) : startPoint.x,
//                               slider.isHorizontal() ? startPoint.y : (float) y);

//         Path backgroundTrack;
//         backgroundTrack.startNewSubPath(startPoint);
//         backgroundTrack.lineTo(endPoint);
//         g.setColour(slider.findColour(Slider::backgroundColourId));
//         g.strokePath(backgroundTrack, {trackWidth, PathStrokeType::curved, PathStrokeType::rounded});

//         Path valueTrack;
//         Point<float> minPoint, maxPoint, thumbPoint;

//         if (isTwoVal || isThreeVal) {
//             minPoint = {slider.isHorizontal() ? minSliderPos : (float) width * 0.5f,
//                         slider.isHorizontal() ? (float) height * 0.5f : minSliderPos};

//             if (isThreeVal)
//                 thumbPoint = {slider.isHorizontal() ? sliderPos : (float) width * 0.5f,
//                               slider.isHorizontal() ? (float) height * 0.5f : sliderPos};

//             maxPoint = {slider.isHorizontal() ? maxSliderPos : (float) width * 0.5f,
//                         slider.isHorizontal() ? (float) height * 0.5f : maxSliderPos};
//         } else {
//             auto kx = slider.isHorizontal() ? sliderPos : ((float) x + (float) width * 0.5f);
//             auto ky = slider.isHorizontal() ? ((float) y + (float) height * 0.5f) : sliderPos;

//             minPoint = startPoint;
//             maxPoint = {kx, ky};
//         }

//         auto thumbWidth = getSliderThumbRadius(slider);
//         auto thumbHeight = jmin(16, slider.isHorizontal() ? static_cast<int>((float) slider.getHeight())
//                                                           : static_cast<int>((float) slider.getWidth()));

//         valueTrack.startNewSubPath(minPoint);
//         valueTrack.lineTo(isThreeVal ? thumbPoint : maxPoint);
//         g.setColour(slider.findColour(Slider::trackColourId));
//         g.strokePath(valueTrack, {trackWidth, PathStrokeType::curved, PathStrokeType::rounded});

//         if (!isTwoVal) {
//             g.setColour(slider.findColour(Slider::thumbColourId));
//             g.fillRoundedRectangle(
//                 Rectangle<float>(static_cast<float>(thumbWidth * 2), static_cast<float>(thumbHeight))
//                                  .withCentre(isThreeVal ? thumbPoint : maxPoint),
//                 trackWidth
//             );
//             // g.fillEllipse(Rectangle<float>(static_cast<float>(thumbWidth), static_cast<float>(thumbWidth))
//             //                   .withCentre(isThreeVal ? thumbPoint : maxPoint));
//         }

//         if (isTwoVal || isThreeVal) {
//             auto sr = jmin(trackWidth, (slider.isHorizontal() ? (float) height : (float) width) * 0.4f);
//             auto pointerColour = slider.findColour(Slider::thumbColourId);

//             if (slider.isHorizontal()) {
//                 drawPointer(g, minSliderPos - sr,
//                             jmax(0.0f, (float) y + (float) height * 0.5f - trackWidth * 2.0f),
//                             trackWidth * 2.0f, pointerColour, 2);

//                 drawPointer(g, maxSliderPos - trackWidth,
//                             jmin((float) (y + height) - trackWidth * 2.0f, (float) y + (float) height * 0.5f),
//                             trackWidth * 2.0f, pointerColour, 4);
//             } else {
//                 drawPointer(g, jmax(0.0f, (float) x + (float) width * 0.5f - trackWidth * 2.0f),
//                             minSliderPos - trackWidth,
//                             trackWidth * 2.0f, pointerColour, 1);

//                 drawPointer(g, jmin((float) (x + width) - trackWidth * 2.0f, (float) x + (float) width * 0.5f),
//                             maxSliderPos - sr,
//                             trackWidth * 2.0f, pointerColour, 3);
//             }
//         }
//         // can not be reached
//         // if (slider.isBar())
//         //     drawLinearSliderOutline(g, x, y, width, height, style, slider);
//     }
// }

TextLayout MoLookAndFeel::layoutTooltipText(TypefaceMetricsKind metrics, const String& text, Colour colour) noexcept {
    const int maxToolTipWidth = 600;
    const float tooltipFontSize = 14.0f;

    AttributedString s;
    s.setJustification(Justification::left);
    s.setLineSpacing(tooltipFontSize * 0.2f); // 1.2 line spacing
    s.append(text, FontOptions(tooltipFontSize).withMetricsKind(metrics), colour);

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

void MoLookAndFeel::drawGroupComponentOutline(Graphics& g, int width, int height,
                                              const String& text, const Justification& position,
                                              GroupComponent& group) {
    const float textH = 11.0f;
    const float fontPtSize = textH;
    const float indent = 3.0f;
    const float textEdgeGap = 4.0f;
    auto cs = 5.0f;

    Font f = getSmallFont(fontPtSize);

    Path p;
    auto x = indent;
    auto y = f.getAscent() - 3.0f;
    auto w = jmax(0.0f, (float) width - x * 2.0f);
    auto h = jmax(0.0f, (float) height - y - indent);
    cs = jmin(cs, w * 0.5f, h * 0.5f);
    auto cs2 = 2.0f * cs;

    auto textW = text.isEmpty() ? 0 : jlimit(0.0f, jmax(0.0f, w - cs2 - textEdgeGap * 2),
                                             (float) GlyphArrangement::getStringWidthInt(f, text) + textEdgeGap * 2.0f);
    auto textX = cs + textEdgeGap;

    if (position.testFlags(Justification::horizontallyCentred)) {
        textX = cs + (w - cs2 - textW) * 0.5f;
    } else if (position.testFlags(Justification::right)) {
        textX = w - cs - textW - textEdgeGap;
    }

    p.startNewSubPath(x + textX + textW, y);
    p.lineTo(x + w - cs, y);

    p.addArc(x + w - cs2, y, cs2, cs2, 0, MathConstants<float>::halfPi);
    p.lineTo(x + w, y + h - cs);

    p.addArc(x + w - cs2, y + h - cs2, cs2, cs2, MathConstants<float>::halfPi, MathConstants<float>::pi);
    p.lineTo(x + cs, y + h);

    p.addArc(x, y + h - cs2, cs2, cs2, MathConstants<float>::pi, MathConstants<float>::pi * 1.5f);
    p.lineTo(x, y + cs);

    p.addArc(x, y, cs2, cs2, MathConstants<float>::pi * 1.5f, MathConstants<float>::twoPi);
    p.lineTo(x + textX, y);

    auto alpha = group.isEnabled() ? 1.0f : 0.5f;

    g.setColour(group.findColour(GroupComponent::outlineColourId).withMultipliedAlpha(alpha));

    g.strokePath(p, PathStrokeType(2.0f));

    g.setColour(group.findColour(GroupComponent::textColourId).withMultipliedAlpha(alpha));
    g.setFont(f);
    g.drawText(text, roundToInt(x + textX), 0, roundToInt(textW), roundToInt(textH), Justification::centred, true);
}

//==============================================================================
// Timeline specific LookAndFeel subclass
//==============================================================================
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