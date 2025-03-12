#pragma once

#include <JuceHeader.h>
#include <common/Utilities.h>  // from Tracktion

#include "Components.h"
#include "LookAndFeel.h"
#include "../../model/PsgClip.h"
#include "../../model/EditUtilities.h"

namespace MoTool {


//==============================================================================
class PsgClipComponent : public MidiClipComponent {
public:
    /**
    | Register | Function                 | Color |
    |----------|--------------------------|-------|
    |     0    | Channel A fine pitch     | A     |
    |     1    | Channel A coarse pitch   | A     |
    |     2    | Channel B fine pitch     | B     |
    |     3    | Channel B coarse pitch   | B     |
    |     4    | Channel C fine pitch     | C     |
    |     5    | Channel C coarse pitch   | C     |
    |     6    | Noise pitch              | Mix   |
    |     7    | Mixer                    | Mix   |
    |     8    | Channel A volume + env   | A     |
    |     9    | Channel B volume + env   | B     |
    |    10    | Channel C volume + env   | C     |
    |    11    | Envelope fine duration   | Env   |
    |    12    | Envelope coarse duration | Env   |
    |    13    | Envelope shape           | Env   |
    */

    struct ColorCoding {
        inline static const std::array<Colour, 14> regColors = {
            Colors::PSG::A,  // red-100
            Colors::PSG::A,  // red-100
            Colors::PSG::B,  // green-100
            Colors::PSG::B,  // green-100
            Colors::PSG::C,  // blue-100
            Colors::PSG::C,  // blue-100
            Colors::PSG::Mix,  // yellow-100
            Colors::PSG::Mix,  // yellow-100
            Colors::PSG::A,  // red-100
            Colors::PSG::B,  // green-100
            Colors::PSG::C,  // blue-100
            Colors::PSG::Env,  // purple-100
            Colors::PSG::Env,  // purple-100
            Colors::PSG::Env   // purple-100
        };
    };

    using MidiClipComponent::MidiClipComponent;

    PsgClip* getPsgClip() {
        return dynamic_cast<PsgClip*>(clip.get());
    }

    void paint(Graphics& g) override {
        ClipComponent::paint(g);

        auto* psgClip = getPsgClip();
        if (psgClip == nullptr) return;

        const auto rect = getLocalBounds();
        const auto clipRange = psgClip->getEditTimeRange();
        const auto viewRange = editViewState.zoom.getRange();
        auto timeToX = [width = rect.getWidth(), clipRange, l = clipRange.getLength()] (auto time) {
            return roundToInt(((time - clipRange.getStart()) * width) / l);
        };
        const auto tc = Helpers::getEditTimecodeFormat(psgClip->edit);
        const auto frameDur = te::TimeDuration::fromSeconds(1.0 / tc.getFPS());
        const auto pixelsPerFrame = frameDur.inSeconds() * rect.getWidth() / clipRange.getLength().inSeconds();

        auto& regs = psgClip->getSequence().getControllerEvents();
        if (regs.size() == 0)
            return;

        const int regsRange = 14;
        const float laneHeight = std::round(static_cast<float>(rect.getHeight()) / regsRange);
        const float left = static_cast<float>(rect.getX());
        g.setFont(12.0f);

        for (auto reg : regs) {
            int regNumber = reg->getType() - 20;

            auto s = reg->getEditTime(*psgClip);
            if (s < clipRange.getStart() || s < viewRange.getStart())
                continue;
            if (s >= clipRange.getEnd() || s >= viewRange.getEnd())
                break;

            float x1 = (float)timeToX(s) - left;
            if (x1 < 0)
                continue;
            float y1 = (static_cast<float>(regNumber) / regsRange) * static_cast<float>(rect.getHeight());

            auto color = ColorCoding::regColors[static_cast<size_t>(regNumber)];
            if (pixelsPerFrame >= 12) {
                String hexValue = choc::text::createHexString(static_cast<uint8_t>(reg->getControllerValue()), 2);
                g.setColour(color.withLightness(0.75f));
                g.fillRect(x1, y1, (float)pixelsPerFrame, (float)laneHeight);
                g.setColour(Colours::black);
                g.drawText(hexValue, (int)(x1 + 0), (int)y1, (int)(pixelsPerFrame), (int)laneHeight, Justification::left, false);
            } else {
                auto value = static_cast<float>(reg->getControllerValue()) / 255.0f;
                g.setColour(color.withLightness(0.75f).withAlpha(0.5f + value / 2.0f));
                g.fillRect(x1, y1, (float)pixelsPerFrame, (float)laneHeight);
            }
        }
        // DBG("Painted " << counter << " registers");
    }

};

}  // namespace MoTool
