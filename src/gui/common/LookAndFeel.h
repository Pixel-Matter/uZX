#pragma once
#include <JuceHeader.h>

namespace MoTool {

namespace Colors {

// Tailwind CSS color palette
// https://tailwindcss.com/docs/customizing-colors
struct Palette {
    inline static const juce::Colour slate50    = juce::Colour::fromString("#0xFFF8FAFC");
    inline static const juce::Colour slate100   = juce::Colour::fromString("#0xFFF1F5F9");
    inline static const juce::Colour slate200   = juce::Colour::fromString("#0xFFE2E8F0");
    inline static const juce::Colour slate300   = juce::Colour::fromString("#0xFFCBD5E1");
    inline static const juce::Colour slate400   = juce::Colour::fromString("#0xFF94A3B8");
    inline static const juce::Colour slate500   = juce::Colour::fromString("#0xFF64748B");
    inline static const juce::Colour slate600   = juce::Colour::fromString("#0xFF475569");
    inline static const juce::Colour slate700   = juce::Colour::fromString("#0xFF334155");
    inline static const juce::Colour slate750   = juce::Colour::fromString("#0xFF283548");
    inline static const juce::Colour slate800   = juce::Colour::fromString("#0xFF1E293B");
    inline static const juce::Colour slate850   = juce::Colour::fromString("#0xFF162032");
    inline static const juce::Colour slate900   = juce::Colour::fromString("#0xFF0F172A");
    inline static const juce::Colour slate950   = juce::Colour::fromString("#0xFF020617");

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
    inline static const juce::Colour backgroundDark= Palette::slate950;  // Even darker background
    inline static const juce::Colour background    = Palette::slate900;  // Main app background
    inline static const juce::Colour backgroundAlt = Palette::slate800;  // Elevated surfaces
    inline static const juce::Colour backgroundSel = Palette::slate750;  // Selected items background
    inline static const juce::Colour surface       = Palette::slate700;  // UI elements
    inline static const juce::Colour surfaceAlt    = Palette::slate400;  // Secondary UI elements

    // Text colors
    inline static const juce::Colour textPrimary   = Palette::slate100;  // Main text
    inline static const juce::Colour textSecondary = Palette::slate400;  // Secondary text
    inline static const juce::Colour textDisabled  = Palette::slate500;  // Disabled text

    // Border and separator colors
    inline static const juce::Colour borderLight   = Palette::slate500;  // Lighter borders
    inline static const juce::Colour border        = Palette::slate600;  // Normal borders
    inline static const juce::Colour borderDark    = Palette::slate700;  // Darker borders

    // Other UI colors
    inline static const juce::Colour primary       = juce::Colour::fromString("#FF0EA5E9"); // sky-500
    inline static const juce::Colour secondary     = juce::Colour::fromString("#FF8B5CF6"); // violet-500
    inline static const juce::Colour success       = juce::Colour::fromString("#FF10B981"); // emerald-500
    inline static const juce::Colour warning       = juce::Colour::fromString("#FFF59E0B"); // amber-500
    inline static const juce::Colour error         = juce::Colour::fromString("#FFEF4444"); // red-500
};

// Timeline specific colors
struct Timeline {
    inline static const juce::Colour grid         = Palette::slate600;
    inline static const juce::Colour cursor       = Theme::primary;
    inline static const juce::Colour clipBg       = juce::Colour::fromString("#FF3B82F6"); // blue-500
    inline static const juce::Colour clipSelected = juce::Colour::fromString("#FF2563EB"); // blue-600

    // Track colors array
    static inline const std::array<juce::Colour, 8> trackColors = {
        Palette::blue500,
        Palette::emerald500,
        Palette::violet500,
        Palette::amber500,
        Palette::pink500,
        Palette::cyan500,
        Palette::lime500,
        Palette::orange500,
    };

    // Track grid tick colors array on background Palette::slate850 or Palette::slate800
    static inline const std::array<juce::Colour, 3> trackGridTickColors = {
        Palette::slate950,  // darker than bg
        Palette::slate700,  // brighter
        Palette::slate500,  // brightest
    };
};

// PSG specific colors
struct PSG {
    inline static const auto A   = Palette::blue500;
    inline static const auto B   = Palette::emerald500;
    inline static const auto C   = Palette::amber500;
    inline static const auto Mix = Palette::cyan500;
    inline static const auto Env = Palette::violet500;
};

} // namespace Colors

class MoLookAndFeel : public juce::LookAndFeel_V4 {
public:

    struct TimelineGridTick {
        int x;
        size_t level; // 0: finest, 1: coarse, 2: coarser
        String label;
    };

    MoLookAndFeel();


    // Helper method to debug current color scheme
    void debugColourScheme();

    // Custom drawing methods
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                            const juce::Colour& backgroundColour,
                            bool shouldDrawButtonAsHighlighted,
                            bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    void drawClip(juce::Graphics& g, const juce::Rectangle<int>& bounds,
                 bool isSelected, const juce::Colour& clipColor);

    static juce::TextLayout layoutTooltipText(juce::TypefaceMetricsKind metrics, const juce::String& text, juce::Colour colour) noexcept;

    juce::Rectangle<int> getTooltipBounds(const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;

    // Custom tooltip drawing
    void drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height) override;

    // Timeline specific drawing methods
    void drawTimelineCursor(juce::Graphics& g, float position, int height);

    void drawTimelineGrid(juce::Graphics& g, const juce::Rectangle<int>& bounds,
                          const std::vector<TimelineGridTick>& ticks) const;

    // Font override methods for different UI elements
    juce::Font getSliderPopupFont(juce::Slider& slider) override;
    juce::Font getNumericReadoutFont() const;

    // Override slider text box creation to set custom font
    juce::Label* createSliderTextBox(juce::Slider& slider) override;

    // Method to setup consistent numeric text editor styling
    void setupNumericTextEditor(juce::TextEditor& editor) const;
};

}  // namespace MoTool
