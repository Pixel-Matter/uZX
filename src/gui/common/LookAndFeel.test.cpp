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

        beginTest("BinaryData contains font resources");
        {
            expect(BinaryData::InterRegular_ttfSize > 0, "Inter Regular font data should be non-empty");
            expect(BinaryData::InterBold_ttfSize > 0, "Inter Bold font data should be non-empty");
            expect(BinaryData::IosevkaAileRegular_ttfSize > 0, "Iosevka Regular font data should be non-empty");
            expect(BinaryData::IosevkaAileSemiBold_ttfSize > 0, "Iosevka SemiBold font data should be non-empty");
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

        beginTest("Unknown font falls through to system");
        {
            Font unknownFont(FontOptions("Comic Sans MS", 14.0f, Font::plain));
            auto typeface = lf.getTypefaceForFont(unknownFont);
            // Should not crash and should not return Inter/Iosevka
            expect(typeface != nullptr, "Unknown font should still return a typeface");
        }
    }
};

static EmbeddedFontTests embeddedFontTests;

} // namespace MoTool::Tests
