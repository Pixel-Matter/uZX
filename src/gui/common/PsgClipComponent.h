#pragma once

#include <JuceHeader.h>
#include <common/Utilities.h>  // from Tracktion

#include "Components.h"
#include "../../model/PsgClip.h"
#include "LookAndFeel.h"
#include "juce_graphics/juce_graphics.h"

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
        const auto frameDur = te::TimeDuration::fromSeconds(1 / 50.0);

        auto timeToX = [width = rect.getWidth(), clipRange, l = clipRange.getLength()] (auto time) {
            return roundToInt(((time - clipRange.getStart()) * width) / l);
        };

        auto& regs = psgClip->getSequence().getControllerEvents();
        if (regs.size() == 0)
            return;

        const int regsRange = 14;
        const float laneHeight = std::round(static_cast<float>(rect.getHeight()) / regsRange);
        const float left = static_cast<float>(rect.getX());

        for (auto reg : regs) {
            int regNumber = reg->getType() - 20;

            auto s = reg->getEditTime(*psgClip);
            if (s < clipRange.getStart() || s < viewRange.getStart())
                continue;
            if (s >= clipRange.getEnd() || s >= viewRange.getEnd())
                break;

            float x1 = (float) timeToX(s) - left;
            if (x1 < 0)
                continue;
            float x2 = (float)timeToX(s + frameDur) - left;
            // float x2 = x1 + 1;

            float y1 = (static_cast<float>(regNumber) / regsRange) * static_cast<float>(rect.getHeight());

            auto color = ColorCoding::regColors[static_cast<size_t>(regNumber)];
            auto value = static_cast<float>(reg->getControllerValue()) / 255.0f;
            color = color.withLightness(0.75f);
            // color = color.withMultipliedLightness(1.0f);
            g.setColour(color);
            // g.fillRect(x1, y1, x2 - x1, laneHeight);
            y1 = y1 + laneHeight * value;
            g.drawLine(x1, y1, x2, y1, 1.0f);
        }
        // DBG("Painted " << counter << " registers");
    }

};

}  // namespace MoTool
