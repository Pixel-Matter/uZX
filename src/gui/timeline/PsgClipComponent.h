#pragma once

#include <JuceHeader.h>
#include <common/Utilities.h>  // from Tracktion

#include "ClipComponents.h"
#include "../common/LookAndFeel.h"
#include "../../model/PsgClip.h"
#include "../../model/EditUtilities.h"

namespace MoTool {

namespace {

static int bisectFindPosition(
    const juce::Array<PsgParamFrame*>& frames,
    const PsgClip& clip,
    te::TimePosition pos
) {
    int low = 0;
    int high = frames.size() - 1;

    while (low <= high) {
        int mid = (low + high) / 2;
        if (frames[mid]->getEditTime(clip) < pos)
            low = mid + 1;
        else
            high = mid - 1;
    }
    return low;
}

} // namespace

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
        paintParameters(g);
        // paintRegisters(g);
    }

    void paintRegisters(Graphics& g) {
        ClipComponent::paint(g);
        g.setFont(12.0f);

        auto* psgClip = getPsgClip();
        if (psgClip == nullptr) return;

        const auto rect = getLocalBounds();
        const auto clipRange = psgClip->getEditTimeRange();
        const auto viewRange = editViewState.zoom.getRange();

        auto timeToX = [w = rect.getWidth(), s = clipRange.getStart(), len = clipRange.getLength(),
                        left = rect.getX()] (auto time) {
            return static_cast<float>(((time - s) * w) / len - left);
        };

        const auto tc = Helpers::getEditTimecodeFormat(psgClip->edit);
        const auto frameDur = te::TimeDuration::fromSeconds(1.0 / tc.getFPS());
        const float pixelsPerFrame = static_cast<float>(frameDur.inSeconds() * rect.getWidth()) / static_cast<float>(clipRange.getLength().inSeconds());

        constexpr auto regsRange = uZX::PsgRegsFrame::size();
        const float laneHeight = std::round(static_cast<float>(rect.getHeight()) / regsRange);

        te::TimePosition startPos = jmax(clipRange.getStart(), viewRange.getStart() - frameDur);
        te::TimePosition endPos = jmin(clipRange.getEnd(), viewRange.getEnd());

        const auto& frames = psgClip->getPsg().getFrames();
        if (frames.size() == 0)
            return;

        const auto startIdx = bisectFindPosition(frames, *psgClip, startPos);

        uZX::PsgRegsFrame regsFrame;
        for (int i = startIdx; i < frames.size(); ++i) {
            const auto& frame = frames[i];
            auto s = frame->getEditTime(*psgClip);

            if (s < startPos)
                continue;
            if (s >= endPos)
                break;

            float x1 = timeToX(s);
            if (x1 + pixelsPerFrame < 0)
                continue;

            const auto& frameData = frame->getData();
            regsFrame.clear();
            frameData.updateRegisters(regsFrame);

            for (size_t regNumber = 0; regNumber < regsFrame.size(); ++regNumber) {
                if (regsFrame.isSet(regNumber)) {
                    auto value = regsFrame.getRaw(regNumber);
                    float y1 = static_cast<float>(regNumber) / regsRange * static_cast<float>(rect.getHeight());

                    auto color = ColorCoding::regColors[static_cast<size_t>(regNumber)];
                    if (pixelsPerFrame >= 12) {
                        String hexValue = choc::text::createHexString(value, 2);
                        g.setColour(color.withLightness(0.75f));
                        g.fillRect(x1, y1, pixelsPerFrame, laneHeight);
                        g.setColour(Colours::black);
                        // it uses GlyphArrangementCache!
                        g.drawSingleLineText(hexValue, (int)(x1 + 1), (int)(y1 + laneHeight - 1), Justification::left);
                    } else {
                        auto val = static_cast<float>(value) / 255.0f;
                        g.setColour(color.withLightness(0.75f).withAlpha(0.5f + val / 2.0f));
                        g.fillRect(x1, y1, pixelsPerFrame, laneHeight);
                    }
                }
            }
        }
    }

    void paintParameters(Graphics& g) {
        ClipComponent::paint(g);
        g.setFont(12.0f);

        auto* psgClip = getPsgClip();
        if (psgClip == nullptr) return;

        const auto rect = getLocalBounds();
        const auto clipRange = psgClip->getEditTimeRange();
        const auto viewRange = editViewState.zoom.getRange();

        auto timeToX = [w = rect.getWidth(), s = clipRange.getStart(), len = clipRange.getLength(),
                        left = rect.getX()] (auto time) {
            return static_cast<float>(((time - s) * w) / len - left);
        };

        const auto tc = Helpers::getEditTimecodeFormat(psgClip->edit);
        const auto frameDur = te::TimeDuration::fromSeconds(1.0 / tc.getFPS());
        const float pixelsPerFrame = frameDur.inSeconds() * rect.getWidth() / clipRange.getLength().inSeconds();

        constexpr auto lanesRange = PsgParamType::size();
        const float laneHeight = std::round(static_cast<float>(rect.getHeight()) / lanesRange);

        te::TimePosition startPos = jmax(clipRange.getStart(), viewRange.getStart() - frameDur);
        te::TimePosition endPos = jmin(clipRange.getEnd(), viewRange.getEnd());

        const auto& frames = psgClip->getPsg().getFrames();
        if (frames.size() == 0)
            return;

        const auto startIdx = bisectFindPosition(frames, *psgClip, startPos);

        for (int i = startIdx; i < frames.size(); ++i) {
            const auto& frame = frames[i];
            jassert(frame != nullptr);
            auto s = frame->getEditTime(*psgClip);

            if (s < startPos)
                continue;
            if (s >= endPos)
                break;

            float x1 = timeToX(s);
            if (x1 + pixelsPerFrame < 0)
                continue;

            const auto& frameData = frame->getData();

            for (int paramNum = 0; paramNum < static_cast<int>(frameData.size()); ++paramNum) {

                if (frameData.isSet(paramNum)) {
                    auto value = frameData.getRaw(static_cast<PsgParamType>(paramNum));
                    float y1 = static_cast<float>(paramNum) / lanesRange * static_cast<float>(rect.getHeight());

                    g.setColour(Colors::PSG::A);
                    auto val = static_cast<float>(value) / 16384.0f;
                    g.drawHorizontalLine(roundToInt(y1 + val / laneHeight), x1, x1 + pixelsPerFrame);
                    // g.fillRect(x1, y1, (float)pixelsPerFrame, laneHeight);
                }
            }
        }
    }


};

}  // namespace MoTool
