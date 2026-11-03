#include <JuceHeader.h>
#include "Oklch.h"
#include "../models/PsgClip.h"
#include "../gui/common/LookAndFeel.h"

namespace MoTool::Oklch {

class OklchTests : public juce::UnitTest {
public:
    OklchTests() : UnitTest("Oklch", "MoTool") {}

    void runTest() override {
        beginTest("Converts a known colour both ways");
        {
            // Tailwind blue-500 = oklch(0.623 0.188 259.815)
            const auto lch = fromColour(Colour(0xff3b82f6));
            expectWithinAbsoluteError(lch.L, 0.623f, 0.001f);
            expectWithinAbsoluteError(lch.C, 0.188f, 0.001f);
            expectWithinAbsoluteError(lch.H, 259.815f, 0.1f);
            expect(toColour({0.623f, 0.188f, 259.815f}) == Colour(0xff3b82f6));
        }

        beginTest("Clamps out-of-gamut chroma without shifting lightness or hue");
        {
            const auto lch = fromColour(toColour({0.7f, 0.4f, 200.0f}));
            expectWithinAbsoluteError(lch.L, 0.7f, 0.005f);
            expectWithinAbsoluteError(lch.H, 200.0f, 1.0f);
            expect(lch.C < 0.4f);
        }

        beginTest("Clip colours share lightness and chroma, differ in hue");
        {
            for (int i = 0; i < 18; ++i) {
                const auto lch = fromColour(PsgClip::colourForTrackIndex(i));
                expectWithinAbsoluteError(lch.L, 0.72f, 0.005f);
                expectWithinAbsoluteError(lch.C, 0.12f, 0.002f);
            }
            expect(PsgClip::colourForTrackIndex(0) != PsgClip::colourForTrackIndex(1));
            expect(PsgClip::colourForTrackIndex(0) == PsgClip::colourForTrackIndex(18));
        }

        beginTest("Accent colours share lightness and chroma");
        {
            for (auto colour : {Colors::PSG::A, Colors::PSG::B, Colors::PSG::C, Colors::PSG::Env,
                                Colors::PSG::AB, Colors::PSG::AC, Colors::PSG::BC,
                                Colors::Theme::primary, Colors::Theme::secondary, Colors::Theme::success,
                                Colors::Theme::warning, Colors::Theme::error, Colors::Meter::midi}) {
                // Hues sRGB can't show that vividly (yellow, cyan, sky) sit on its boundary
                const auto lch = fromColour(colour);
                expectWithinAbsoluteError(lch.L, 0.68f, 0.005f);
                expectWithinAbsoluteError(lch.C, jmin(0.168f, maxChroma(0.68f, lch.H)), 0.003f);
            }
        }
    }
};

static OklchTests oklchTests;

} // namespace MoTool::Oklch
