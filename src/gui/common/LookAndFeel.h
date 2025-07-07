#pragma once
#include <JuceHeader.h>

namespace MoTool {

namespace Colors {
struct Slate {
    inline static const juce::Colour slate50  = juce::Colour::fromString("#FFF8FAFC");
    inline static const juce::Colour slate100 = juce::Colour::fromString("#FFF1F5F9");
    inline static const juce::Colour slate200 = juce::Colour::fromString("#FFE2E8F0");
    inline static const juce::Colour slate300 = juce::Colour::fromString("#FFCBD5E1");
    inline static const juce::Colour slate400 = juce::Colour::fromString("#FF94A3B8");
    inline static const juce::Colour slate500 = juce::Colour::fromString("#FF64748B");
    inline static const juce::Colour slate600 = juce::Colour::fromString("#FF475569");
    inline static const juce::Colour slate700 = juce::Colour::fromString("#FF334155");
    inline static const juce::Colour slate800 = juce::Colour::fromString("#FF1E293B");
    inline static const juce::Colour slate900 = juce::Colour::fromString("#FF0F172A");
    inline static const juce::Colour slate950 = juce::Colour::fromString("#FF020617");

    inline static const juce::Colour blue500    = juce::Colour::fromString("#0xFF3B82F6");
    inline static const juce::Colour emerald500 = juce::Colour::fromString("#0xFF10B981");
    inline static const juce::Colour violet500  = juce::Colour::fromString("#0xFF8B5CF6");
    inline static const juce::Colour amber500   = juce::Colour::fromString("#0xFFF59E0B");
    inline static const juce::Colour pink500    = juce::Colour::fromString("#0xFFEC4899");
    inline static const juce::Colour cyan500    = juce::Colour::fromString("#0xFF06B6D4");
    inline static const juce::Colour lime500    = juce::Colour::fromString("#0xFF84CC16");
    inline static const juce::Colour orange500  = juce::Colour::fromString("#0xFFF97316");
};

// Main theme colors using slate
struct Theme {
    // Base colors
    inline static const juce::Colour background    = Slate::slate900;  // Main app background
    inline static const juce::Colour backgroundAlt = Slate::slate800;  // Elevated surfaces
    inline static const juce::Colour surface       = Slate::slate700;  // UI elements
    inline static const juce::Colour surfaceAlt    = Slate::slate400;  // Secondary UI elements

    // Text colors
    inline static const juce::Colour textPrimary   = Slate::slate100;  // Main text
    inline static const juce::Colour textSecondary = Slate::slate400;  // Secondary text
    inline static const juce::Colour textDisabled  = Slate::slate500;  // Disabled text

    // Border and separator colors
    inline static const juce::Colour border        = Slate::slate600;  // Normal borders
    inline static const juce::Colour borderLight   = Slate::slate500;  // Lighter borders
    inline static const juce::Colour borderDark    = Slate::slate700;  // Darker borders

    // Other UI colors
    inline static const juce::Colour primary       = juce::Colour::fromString("#FF0EA5E9"); // sky-500
    inline static const juce::Colour secondary     = juce::Colour::fromString("#FF8B5CF6"); // violet-500
    inline static const juce::Colour success       = juce::Colour::fromString("#FF10B981"); // emerald-500
    inline static const juce::Colour warning       = juce::Colour::fromString("#FFF59E0B"); // amber-500
    inline static const juce::Colour error         = juce::Colour::fromString("#FFEF4444"); // red-500
};

// Timeline specific colors
struct Timeline {
    inline static const juce::Colour grid         = Slate::slate600;
    inline static const juce::Colour cursor       = Theme::primary;
    inline static const juce::Colour clipBg       = juce::Colour::fromString("#FF3B82F6"); // blue-500
    inline static const juce::Colour clipSelected = juce::Colour::fromString("#FF2563EB"); // blue-600

    // Track colors array
    static inline const std::array<juce::Colour, 8> trackColors = {
        Slate::blue500,
        Slate::emerald500,
        Slate::violet500,
        Slate::amber500,
        Slate::pink500,
        Slate::cyan500,
        Slate::lime500,
        Slate::orange500,
    };
};

// PSG specific colors
struct PSG {
    inline static const auto A   = Slate::blue500;
    inline static const auto B   = Slate::emerald500;
    inline static const auto C   = Slate::amber500;
    inline static const auto Mix = Slate::cyan500;
    inline static const auto Env = Slate::violet500;
};

} // namespace Colors

class MoLookAndFeel : public juce::LookAndFeel_V4 {
public:

    MoLookAndFeel() {
        using namespace Colors;
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
        setColour(juce::TextEditor::backgroundColourId, Theme::surface);
        setColour(juce::TextEditor::textColourId, Theme::textPrimary);
        // setColour(juce::TextEditor::highlightColourId, Theme::backgroundAlt);
        setColour(juce::TextEditor::highlightColourId, Theme::primary.withAlpha(0.6f));
        setColour(juce::TextEditor::highlightedTextColourId, Theme::background);
        setColour(juce::TextEditor::outlineColourId, Theme::border);
        setColour(juce::TextEditor::focusedOutlineColourId, Theme::primary);
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

        // Labels (for editable labels)
        setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::Label::textColourId, Theme::textPrimary);
        setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Label::backgroundWhenEditingColourId, Theme::surface);
        setColour(juce::Label::textWhenEditingColourId, Theme::textPrimary);
        setColour(juce::Label::outlineWhenEditingColourId, Theme::primary);

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

    // Helper method to debug current color scheme
    void debugColourScheme() {
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

    // Custom drawing methods
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                            const juce::Colour& backgroundColour,
                            bool shouldDrawButtonAsHighlighted,
                            bool shouldDrawButtonAsDown) override {
        auto bounds = button.getLocalBounds().toFloat();
        auto cornerSize = 4.0f;

        g.setColour(shouldDrawButtonAsDown ? backgroundColour.darker(0.2f) :
                    shouldDrawButtonAsHighlighted ? backgroundColour.brighter(0.1f) :
                    backgroundColour);

        g.fillRoundedRectangle(bounds, cornerSize);
    }

    // Timeline specific drawing methods
    void drawTimelineBackground(juce::Graphics& g, const juce::Rectangle<int>& bounds) {
        // NOTE example code, not actual implementation
        g.fillAll(Colors::Theme::background);

        // Draw vertical grid lines
        g.setColour(Colors::Timeline::grid);
        auto gridSize = 60; // pixels between grid lines
        for (int x = 0; x < bounds.getWidth(); x += gridSize) {
            g.drawVerticalLine(x, 0.0f, (float)bounds.getHeight());
        }
    }

    void drawTimelineCursor(juce::Graphics& g, float position, int height) {
        g.setColour(Colors::Timeline::cursor);
        g.drawVerticalLine((int)position, 0.0f, (float)height);
    }

    void drawClip(juce::Graphics& g, const juce::Rectangle<int>& bounds,
                 bool isSelected, const juce::Colour& clipColor) {
        auto cornerSize = 3.0f;
        g.setColour(isSelected ? Colors::Timeline::clipSelected : clipColor);
        g.fillRoundedRectangle(bounds.toFloat(), cornerSize);

        // Draw clip border
        g.setColour(isSelected ? Colors::Timeline::clipSelected.brighter() : clipColor.brighter());
        g.drawRoundedRectangle(bounds.toFloat(), cornerSize, 1.0f);
    }
};

}  // namespace MoTool
