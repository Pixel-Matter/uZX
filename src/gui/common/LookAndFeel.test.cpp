#include <JuceHeader.h>
#include "LookAndFeel.h"
#include "BinaryData.h"

namespace MoTool::Tests {

class EmbeddedFontTests : public UnitTest {
public:
    EmbeddedFontTests() : UnitTest("EmbeddedFonts", "MoTool") {}

    void runTest() override {
        MoLookAndFeel lf;

        beginTest("Buttons too short for text still get a valid font");
        {
            TextButton button;
            for (int height : {0, 4, 10})
                expect(lf.getTextButtonFont(button, height).getHeightInPoints() > 0.0f,
                       "button " + String(height) + " high");
        }

        beginTest("A knob squeezed below its body draws nothing");
        {
            Slider knob(Slider::RotaryVerticalDrag, Slider::NoTextBox);
            knob.setLookAndFeel(&lf);
            const auto isBlank = [](const Image& image) {
                for (int y = 0; y < image.getHeight(); ++y)
                    for (int x = 0; x < image.getWidth(); ++x)
                        if (! image.getPixelAt(x, y).isTransparent())
                            return false;
                return true;
            };
            knob.setSize(78, 4);
            expect(isBlank(knob.createComponentSnapshot(knob.getLocalBounds())), "4 high");
            knob.setSize(40, 40);
            expect(! isBlank(knob.createComponentSnapshot(knob.getLocalBounds())), "40 high");
            knob.setLookAndFeel(nullptr);
        }

        beginTest("Text sized by its control snaps down to the type scale");
        {
            expectEquals(Fonts::fit(59.0f), Fonts::title);
            expectEquals(Fonts::fit(13.3f), Fonts::body);
            expectEquals(Fonts::fit(10.7f), Fonts::caption);
            expectEquals(Fonts::fit(8.0f), 8.0f);
            TextButton button;
            expectEquals(lf.getTextButtonFont(button, 64).getHeightInPoints(), Fonts::body, "a tall button in a row of controls");
            TextButton large;
            MoLookAndFeel::setLargeText(large);
            expectEquals(lf.getTextButtonFont(large, 64).getHeightInPoints(), Fonts::title, "a channel's letter");
            expectEquals(lf.getTextButtonFont(large, 20).getHeightInPoints(), Fonts::body, "a short one");
            expectEquals(lf.getTextButtonFont(button, 20).getHeightInPoints(), Fonts::body, "AY");
            expectEquals(lf.getPopupMenuFont().getHeightInPoints(), Fonts::menu, "menus");
        }

        beginTest("Selected text reads on its selection, with and without the keyboard focus");
        {
            const auto luminance = [](Colour c) {
                const auto lin = [](float v) { return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f); };
                return 0.2126f * lin(c.getFloatRed()) + 0.7152f * lin(c.getFloatGreen()) + 0.0722f * lin(c.getFloatBlue());
            };
            const auto contrast = [&](Colour a, Colour b) {
                const auto la = luminance(a), lb = luminance(b);
                return (jmax(la, lb) + 0.05f) / (jmin(la, lb) + 0.05f);
            };
            const auto text = lf.findColour(TextEditor::highlightedTextColourId);
            const auto selection = lf.findColour(TextEditor::highlightColourId);
            for (const float alpha : {1.0f, 0.5f})  // JUCE's own dimming of an unfocused editor's selection
                expect(contrast(text, Colors::Theme::background.overlaidWith(selection.withMultipliedAlpha(alpha))) >= 4.5f,
                       "at alpha " + String(alpha));
        }

        beginTest("BinaryData contains font resources");
        {
            expect(BinaryData::InterRegular_ttfSize > 0, "Inter Regular font data should be non-empty");
            expect(BinaryData::InterBold_ttfSize > 0, "Inter Bold font data should be non-empty");
            expect(BinaryData::IosevkaAileRegular_ttfSize > 0, "Iosevka Regular font data should be non-empty");
            expect(BinaryData::IosevkaAileSemiBold_ttfSize > 0, "Iosevka SemiBold font data should be non-empty");
            expect(BinaryData::IosevkaFixedRegular_ttfSize > 0, "Iosevka Fixed font data should be non-empty");
        }

        beginTest("Default sans-serif resolves to embedded Inter");
        {
            Font defaultFont(FontOptions{});
            auto typeface = lf.getTypefaceForFont(defaultFont);
            expect(typeface != nullptr, "Default font typeface should not be null");
            expect(typeface->getName().containsIgnoreCase("Inter"),
                   "Default font should resolve to Inter, got: " + typeface->getName());
        }

        beginTest("Inter font request returns embedded typeface");
        {
            Font interFont(FontOptions("Inter", 14.0f, Font::plain));
            auto typeface = lf.getTypefaceForFont(interFont);
            expect(typeface != nullptr, "Inter typeface should not be null");
            expect(typeface->getName().containsIgnoreCase("Inter"),
                   "Should be Inter, got: " + typeface->getName());
        }

        beginTest("Iosevka Aile Regular returns embedded typeface");
        {
            Font iosevkaFont(FontOptions("Iosevka Aile", 10.0f, Font::plain));
            auto typeface = lf.getTypefaceForFont(iosevkaFont);
            expect(typeface != nullptr, "Iosevka Regular typeface should not be null");
            expect(typeface->getName().containsIgnoreCase("Iosevka"),
                   "Should be Iosevka, got: " + typeface->getName());
        }

        beginTest("Iosevka Aile Semibold returns distinct embedded typeface");
        {
            Font iosevkaRegular(FontOptions("Iosevka Aile", 20.0f, Font::plain));
            Font iosevkaSemibold(FontOptions("Iosevka Aile", "Semibold", 20.0f));

            auto regularTf = lf.getTypefaceForFont(iosevkaRegular);
            auto semiboldTf = lf.getTypefaceForFont(iosevkaSemibold);

            expect(regularTf != nullptr && semiboldTf != nullptr,
                   "Both Iosevka typefaces should not be null");
            expect(regularTf != semiboldTf,
                   "Regular and Semibold should be different typeface objects");
        }

        beginTest("Embedded typefaces are same objects across calls (cached)");
        {
            Font font1(FontOptions("Inter", 12.0f, Font::plain));
            Font font2(FontOptions("Inter", 24.0f, Font::plain));
            auto tf1 = lf.getTypefaceForFont(font1);
            auto tf2 = lf.getTypefaceForFont(font2);
            expect(tf1 == tf2, "Same embedded typeface should be returned regardless of size");
        }

        beginTest("Inter Bold returns distinct embedded typeface");
        {
            Font interRegular(FontOptions("Inter", 14.0f, Font::plain));
            Font interBold(FontOptions("Inter", "Bold", 14.0f));

            auto regularTf = lf.getTypefaceForFont(interRegular);
            auto boldTf    = lf.getTypefaceForFont(interBold);

            expect(regularTf != nullptr && boldTf != nullptr,
                   "Both Inter typefaces should not be null");
            expect(regularTf != boldTf,
                   "Regular and Bold should be different typeface objects");
        }

        beginTest("Any font name resolves to an embedded typeface");
        {
            const auto nameOf = [&lf](const FontOptions& options) {
                return lf.getTypefaceForFont(Font(options))->getName();
            };
            expectEquals(nameOf(FontOptions(Font::getDefaultMonospacedFontName(), 12.0f, Font::plain)),
                         String("Iosevka Fixed"));
            expectEquals(nameOf(FontOptions("Iosevka Fixed", 12.0f, Font::plain)), String("Iosevka Fixed"));
            for (const auto* name : {"Comic Sans MS", "Menlo", "<Serif>"})
                expect(nameOf(FontOptions(name, 14.0f, Font::plain)).containsIgnoreCase("Inter"), name);
        }

        beginTest("The embedded fonts have the characters the UI draws");
        {
            // Non-ASCII characters of the UI strings; one missing would come from a system font
            const auto has = [this, &lf](const FontOptions& options, const String& chars) {
                const auto typeface = lf.getTypefaceForFont(Font(options));
                for (auto c : chars)
                    expect(typeface->getNominalGlyphForCodepoint(c).has_value(),
                           typeface->getName() + " " + typeface->getStyle() + " lacks U+"
                               + String::toHexString((int) c).toUpperCase());
            };
            // flat, natural, sharp, ellipsis, em and en dash, warning, cent, mu, times, copyright
            const auto inter = String::fromUTF8("\xE2\x99\xAD\xE2\x99\xAE\xE2\x99\xAF\xE2\x80\xA6\xE2\x80\x94\xE2\x80\x93\xE2\x9A\xA0\xC2\xA2\xCE\xBC\xC3\x97\xC2\xA9");
            has(FontOptions("Inter", 14.0f, Font::plain), inter);
            has(FontOptions("Inter", "Bold", 14.0f), inter);
            has(FontOptions(Font::getDefaultMonospacedFontName(), 12.0f, Font::plain),
                String::fromUTF8("\xE2\x99\xAD\xE2\x99\xAE\xE2\x99\xAF\xE2\x80\xA6\xC2\xA2\xCE\xBC"));
        }
    }
};

static EmbeddedFontTests embeddedFontTests;

} // namespace MoTool::Tests
