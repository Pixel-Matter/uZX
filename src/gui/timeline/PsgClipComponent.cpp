#include <JuceHeader.h>

#include "PsgClipComponent.h"
#include "../common/LookAndFeel.h"
#include "../../models/EditUtilities.h"

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

const std::array<Colour, 14> RegColors = {
    Colors::PSG::A,    // red-100
    Colors::PSG::A,    // red-100
    Colors::PSG::B,    // green-100
    Colors::PSG::B,    // green-100
    Colors::PSG::C,    // blue-100
    Colors::PSG::C,    // blue-100
    Colors::PSG::Mix,  // yellow-100
    Colors::PSG::Mix,  // yellow-100
    Colors::PSG::A,    // red-100
    Colors::PSG::B,    // green-100
    Colors::PSG::C,    // blue-100
    Colors::PSG::Env,  // purple-100
    Colors::PSG::Env,  // purple-100
    Colors::PSG::Env   // purple-100
};

/** Draw dotted pattern for noise modulation */
void drawNoisePattern(Graphics& g, float x, float y, float width, float height) {
    g.setColour(Colours::black);
    constexpr float dotSpacing = 4.0f;
    constexpr float dotRadius = 1.0f;
    for (float dx = dotSpacing / 2; dx < width - dotSpacing / 2; dx += dotSpacing) {
        for (float dy = dotSpacing / 2; dy < height - dotSpacing / 2; dy += dotSpacing) {
            g.fillEllipse(x + dx - dotRadius, y + dy - dotRadius,
                         dotRadius * 2, dotRadius * 2);
        }
    }
}

/** Draw stripe pattern for envelope modulation based on envelope shape direction */
void drawEnvelopeStripes(Graphics& g, float x, float y, float width, float height, uint8_t shape) {
    g.setColour(Colours::black);
    constexpr float stripeSpacing = 3.0f;
    constexpr float strokeWidth = 1.0f;

    // Determine direction based on envelope shape:
    // Up-first shapes: 4-7, C-F (attack first)
    // Down-first shapes: 0-3, 8-B (decay first)
    // Triangle shapes: A (down-up), E (up-down)
    bool isUpFirst = (shape >= 4 && shape <= 7) || (shape >= 0xC);
    bool isTriangle = (shape == 0xA || shape == 0xE);

    Path stripes;
    if (isTriangle) {
        // Zigzag pattern
        for (float dx = 0; dx < width + height; dx += stripeSpacing * 2) {
            float startX = x + dx;
            // Up stroke
            stripes.startNewSubPath(startX, y + height);
            stripes.lineTo(startX + stripeSpacing, y);
            // Down stroke
            stripes.lineTo(startX + stripeSpacing * 2, y + height);
        }
    } else if (isUpFirst) {
        // Forward stripes //// (attack)
        for (float offset = -height; offset < width + height; offset += stripeSpacing) {
            stripes.startNewSubPath(x + offset, y + height);
            stripes.lineTo(x + offset + height, y);
        }
    } else {
        // Backward stripes \\\\ (decay)
        for (float offset = -height; offset < width + height; offset += stripeSpacing) {
            stripes.startNewSubPath(x + offset, y);
            stripes.lineTo(x + offset + height, y + height);
        }
    }

    // Clip to note bounds
    g.saveState();
    g.reduceClipRegion(juce::Rectangle<float>(x, y, width, height).toNearestIntEdges());
    g.strokePath(stripes, PathStrokeType(strokeWidth));
    g.restoreState();
}

} // namespace

PsgClip* PsgClipComponent::getPsgClip() {
    return dynamic_cast<PsgClip*>(clip.get());
}

void PsgClipComponent::paint(Graphics& g) {
    ClipComponent::paint(g);

    auto lastInterval = paintMeasurer_.getInstantIntervalMs();

    GUIPaintMeasurer::ScopedTimer timer(paintMeasurer_);

    paintParameters(g);
    // paintRegisters(g);

    paintMeasurer_.drawOverlay(g, getLocalBounds());
}

void PsgClipComponent::paintRegisters(Graphics& g) {
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

                auto color = RegColors[static_cast<size_t>(regNumber)];
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

void PsgClipComponent::paintParameters(Graphics& g) {
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
    const auto frameDur = te::TimeDuration::fromSeconds(1.0f / tc.getFPS());
    const float pixelsPerFrame = static_cast<float>(frameDur.inSeconds() * rect.getWidth() / clipRange.getLength().inSeconds());

    // Note height = one semitone (8 octaves = 96 semitones displayed)
    constexpr float pitchRangeInSemitones = 96.0f;
    const float noteHeight = static_cast<float>(rect.getHeight()) / pitchRangeInSemitones;

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

        // Channel colors (A, B, C)
        static const juce::Colour channelColors[] = {
            Colors::PSG::A.withSaturation(1.0f),
            Colors::PSG::B.withSaturation(1.0f),
            Colors::PSG::C.withSaturation(1.0f),
        };

        // Get envelope shape once per frame (used for envelope stripes)
        uint8_t envShape = static_cast<uint8_t>(frameData.getRaw(PsgParamType::EnvelopeShape));

        for (int ch = 0; ch < 3; ++ch) {
            PsgParamType periodType (PsgParamType::TonePeriodA + ch);
            PsgParamType volumeType (PsgParamType::VolumeA + ch);
            PsgParamType toneOnType (PsgParamType::ToneIsOnA + ch);
            PsgParamType envOnType  (PsgParamType::EnvelopeIsOnA + ch);
            PsgParamType noiseOnType(PsgParamType::NoiseIsOnA + ch);

            // Use accumulated state - getRaw returns full state even if mask not set
            const auto rawVolume = frameData.getRaw(volumeType);

            bool toneIsOn = frameData.getRaw(toneOnType) > 0;
            bool hasEnvMod = frameData.getRaw(envOnType) > 0;
            bool hasNoiseMod = frameData.getRaw(noiseOnType) > 0;
            bool isAudible = (rawVolume > 0) || hasEnvMod;

            // Draw note if tone is on and channel is audible (has volume or envelope)
            if (toneIsOn && isAudible) {

                auto period = frameData.getRaw(periodType);
                float pitch = periodType.valueToNormalized(period);
                // If envelope modulated, use max alpha (envelope controls volume)
                float alpha = hasEnvMod ? 1.0f : (static_cast<float>(rawVolume) / 15.0f * 0.8f + 0.2f);
                float noteY = (1.0f - pitch) * rect.getHeight() - noteHeight;
                const auto& color = channelColors[ch];

                // Draw base note rectangle
                g.setColour(color.withAlpha(alpha));
                g.fillRect(x1, noteY, (float)pixelsPerFrame, noteHeight);

                // Draw envelope stripes if modulated
                if (hasEnvMod) {
                    drawEnvelopeStripes(g, x1, noteY, (float)pixelsPerFrame, noteHeight, envShape);
                }

                // Draw noise pattern if modulated
                if (hasNoiseMod) {
                    drawNoisePattern(g, x1, noteY, (float)pixelsPerFrame, noteHeight);
                }
            }
        }

        // Envelope period - render when any channel uses envelope modulation
        bool anyEnvMod = frameData.getRaw(PsgParamType::EnvelopeIsOnA) > 0 ||
                         frameData.getRaw(PsgParamType::EnvelopeIsOnB) > 0 ||
                         frameData.getRaw(PsgParamType::EnvelopeIsOnC) > 0;
        if (anyEnvMod) {
            PsgParamType envType(PsgParamType::EnvelopePeriod);
            auto value = frameData.getRaw(envType);
            float val = envType.valueToNormalized(value);
            g.setColour(Colors::PSG::Env.withSaturation(1.0f).withAlpha(0.75f));
            g.fillRect(x1, (1.0f - val) * rect.getHeight() - noteHeight, (float)pixelsPerFrame, noteHeight);
        }
    }

    // Draw channel color legend at upper-left corner
    {
        constexpr float pad = 3.0f;
        constexpr float swatchSize = 12.0f;
        constexpr float spacing = 1.0f;

        struct LegendItem { const char* label; Colour color; };
        const LegendItem items[] = {
            { "A", Colors::PSG::A.withSaturation(1.0f) },
            { "B", Colors::PSG::B.withSaturation(1.0f) },
            { "C", Colors::PSG::C.withSaturation(1.0f) },
            { "E", Colors::PSG::Env.withSaturation(1.0f) },
        };

        g.setFont(Font(FontOptions(swatchSize - 1.0f).withStyle("Bold")));
        float x = rect.getX() + pad;
        float y = rect.getY() + pad;

        for (const auto& item : items) {
            g.setColour(item.color);
            g.fillRect(x, y, swatchSize, swatchSize);
            g.setColour(Colours::black);
            g.drawText(item.label, (int)x, (int)y, (int)swatchSize, (int)swatchSize, Justification::centred);
            x += swatchSize + spacing;
        }

        // Clip name after legend
        x += pad;
        g.setColour(Colours::white.withAlpha(0.7f));
        g.drawText(psgClip->getName(), (int)x, (int)y, rect.getWidth() - (int)x, (int)swatchSize, Justification::centredLeft);
    }
}

}  // namespace MoTool
