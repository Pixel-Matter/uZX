#pragma once
#include <JuceHeader.h>

namespace MoTool {

namespace Colors {

// Tailwind CSS color palette
// https://tailwindcss.com/docs/customizing-colors
struct Palette {
    inline static const Colour slate50    = Colour::fromString("#0xFFF8FAFC");
    inline static const Colour slate100   = Colour::fromString("#0xFFF1F5F9");
    inline static const Colour slate200   = Colour::fromString("#0xFFE2E8F0");
    inline static const Colour slate300   = Colour::fromString("#0xFFCBD5E1");
    inline static const Colour slate400   = Colour::fromString("#0xFF94A3B8");
    inline static const Colour slate500   = Colour::fromString("#0xFF64748B");
    inline static const Colour slate600   = Colour::fromString("#0xFF475569");
    inline static const Colour slate700   = Colour::fromString("#0xFF334155");
    inline static const Colour slate750   = Colour::fromString("#0xFF283548");
    inline static const Colour slate800   = Colour::fromString("#0xFF1E293B");
    inline static const Colour slate850   = Colour::fromString("#0xFF162032");
    inline static const Colour slate900   = Colour::fromString("#0xFF0F172A");
    inline static const Colour slate950   = Colour::fromString("#0xFF020617");

    inline static const Colour red500     = Colour::fromString("#0xFFEF4444");
    inline static const Colour orange500  = Colour::fromString("#0xFFF97316");
    inline static const Colour amber500   = Colour::fromString("#0xFFF59E0B");
    inline static const Colour yellow500  = Colour::fromString("#0xFFF59E0B");
    inline static const Colour lime500    = Colour::fromString("#0xFF84CC16");
    inline static const Colour green500   = Colour::fromString("#0xFF22C55E");
    inline static const Colour emerald500 = Colour::fromString("#0xFF10B981");
    inline static const Colour teal500    = Colour::fromString("#0xFF14B8A6");
    inline static const Colour cyan500    = Colour::fromString("#0xFF06B6D4");
    inline static const Colour sky500     = Colour::fromString("#0xFF0EA5E9");
    inline static const Colour blue500    = Colour::fromString("#0xFF3B82F6");
    inline static const Colour indigo500  = Colour::fromString("#0xFF6366F1");
    inline static const Colour violet500  = Colour::fromString("#0xFF8B5CF6");
    inline static const Colour purple500  = Colour::fromString("#0xFFA855F7");
    inline static const Colour fuchsia500 = Colour::fromString("#0xFFD946EF");
    inline static const Colour pink500    = Colour::fromString("#0xFFEC4899");
    inline static const Colour rose500    = Colour::fromString("#0xFFFB7185");
    inline static const Colour gray500    = Colour::fromString("#0xFF6B7280");
    inline static const Colour zink500    = Colour::fromString("#0xFF71717A");
    inline static const Colour neutral500 = Colour::fromString("#0xFF737373");
    inline static const Colour stone500   = Colour::fromString("#0xFF7C7C8A");
};

// Main theme colors using slate
struct Theme {
    // Base colors
    inline static const Colour backgroundDark= Palette::slate950;  // Even darker background
    inline static const Colour background    = Palette::slate900;  // Main app background
    inline static const Colour backgroundAlt = Palette::slate800;  // Elevated surfaces
    inline static const Colour backgroundSel = Palette::slate750;  // Selected items background
    inline static const Colour surface       = Palette::slate700;  // UI elements
    inline static const Colour surfaceElevated= Palette::slate500; // Elevated UI elements
    inline static const Colour surfaceAlt    = Palette::slate400;  // Secondary UI elements

    // Text colors
    inline static const Colour textPrimary   = Palette::slate200;  // Main text
    inline static const Colour textSecondary = Palette::slate400;  // Secondary text
    inline static const Colour textDisabled  = Palette::slate500;  // Disabled text

    // Border and separator colors
    inline static const Colour borderLight   = Palette::slate500;  // Lighter borders
    inline static const Colour border        = Palette::slate600;  // Normal borders
    inline static const Colour borderDark    = Palette::slate700;  // Darker borders

    // Other UI colors
    inline static const Colour primary       = Colour::fromString("#FF0EA5E9"); // sky-500
    inline static const Colour secondary     = Colour::fromString("#FF8B5CF6"); // violet-500
    inline static const Colour success       = Colour::fromString("#FF10B981"); // emerald-500
    inline static const Colour soloed        = Colour::fromString("#FF10B981"); // emerald-500
    inline static const Colour warning       = Colour::fromString("#FFF59E0B"); // amber-500
    inline static const Colour error         = Colour::fromString("#FFEF4444"); // red-500
    inline static const Colour muted         = Colour::fromString("#FFEF4444"); // red-500
};

// Timeline specific colors
struct Timeline {
    inline static const Colour grid         = Palette::slate600;
    inline static const Colour cursor       = Theme::primary;
    inline static const Colour clipBg       = Colour::fromString("#FF3B82F6"); // blue-500
    inline static const Colour clipSelected = Colour::fromString("#FF2563EB"); // blue-600

    // Track colors array
    static inline const std::array<Colour, 8> trackColors = {
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
    static inline const std::array<Colour, 3> trackGridTickColors = {
        Palette::slate950,  // darker than bg
        Palette::slate700,  // brighter
        Palette::slate500,  // brightest
    };
};

// PSG specific colors
struct PSG {
    inline static const auto A   = Palette::amber500.withSaturation(1.0f);
    inline static const auto B   = Palette::emerald500.withSaturation(1.0f);
    inline static const auto C   = Palette::sky500.withSaturation(1.0f);
    inline static const auto Env = Palette::purple500.withSaturation(1.0f);
    inline static const auto Mix = Palette::gray500;
};

} // namespace Colors

class MoLookAndFeel : public LookAndFeel_V4 {
public:

    struct TimelineGridTick {
        int x;
        size_t level; // 0: finest, 1: coarse, 2: coarser
        String label;
    };

    MoLookAndFeel();

    Typeface::Ptr getTypefaceForFont(const Font& font) override;

private:
    Typeface::Ptr interRegularTypeface_;
    Typeface::Ptr interBoldTypeface_;
    Typeface::Ptr iosevkaRegularTypeface_;
    Typeface::Ptr iosevkaSemiBoldTypeface_;

public:


    // Helper method to debug current color scheme
    void debugColourScheme();

    // Custom drawing methods
    void drawButtonBackground(Graphics& g, Button& button,
                            const Colour& backgroundColour,
                            bool shouldDrawButtonAsHighlighted,
                            bool shouldDrawButtonAsDown) override;

    Font getSmallFont(float pointHeigth);

    Font getComboBoxFont(ComboBox& box) override;

    float getPopupMenuItemFontProportion();

    Font getPopupMenuFont() override;

    void drawPopupMenuItem(Graphics& g, const Rectangle<int>& area,
                           const bool isSeparator, const bool isActive, const bool isHighlighted,
                           const bool isTicked, const bool hasSubMenu,
                           const String& text, const String& shortcutKeyText,
                           const Drawable* icon, const Colour* const textColourToUse) override;

    PopupMenu::Options getOptionsForComboBoxPopupMenu(ComboBox& box, Label& label) override;

    float getComboBoxPopupMenuItemHeight(Label& label);

    void getIdealPopupMenuItemSize(const String& text, const bool isSeparator,
                                  int standardMenuItemHeight,
                                  int& idealWidth, int& idealHeight) override;

    Font getTextButtonFont(TextButton&, int buttonHeight) override;

    void drawButtonText(Graphics& g,
                        TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    void drawClip(Graphics& g, const Rectangle<int>& bounds,
                 bool isSelected, const Colour& clipColor);

    static TextLayout layoutTooltipText(TypefaceMetricsKind metrics, const String& text, Colour colour) noexcept;

    Rectangle<int> getTooltipBounds(const String& tipText, Point<int> screenPos, Rectangle<int> parentArea) override;

    // Slider specific LookAndFeel overrides
    void drawRotarySlider(Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, Slider& slider) override;

    int getSliderThumbRadius(Slider& slider) override;

    // void drawLinearSlider(Graphics& g, int x, int y, int width, int height,
    //                       float sliderPos, float minSliderPos, float maxSliderPos,
    //                       const Slider::SliderStyle style, Slider& slider) override;

    void drawGroupComponentOutline(Graphics& g, int width, int height,
                                   const String& text, const Justification& position, GroupComponent& group) override;

    // Custom tooltip drawing
    void drawTooltip(Graphics& g, const String& text, int width, int height) override;

    // Timeline specific drawing methods
    void drawTimelineCursor(Graphics& g, float position, int height);

    void drawTimelineGrid(Graphics& g, const Rectangle<int>& bounds,
                          const std::vector<TimelineGridTick>& ticks) const;

};

//==============================================================================
// Readout specific LookAndFeel subclass
//==============================================================================
class ReadoutLookAndFeel : public MoLookAndFeel {
public:
    ReadoutLookAndFeel();

    // Font override methods for different UI elements
    Font getSliderPopupFont(Slider& slider) override;
    Label* createSliderTextBox(Slider& slider) override;

    Font getNumericReadoutFont() const;
    void setupReadoutLabel(Label& label);
    void setupNumericTextEditor(TextEditor& editor) const;
};

}  // namespace MoTool
