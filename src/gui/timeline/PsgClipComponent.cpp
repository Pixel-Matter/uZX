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

/** Draw random dotted pattern for noise modulation, seeded from note position */
void drawNoisePattern(Graphics& g, float x, float y, float width, float height, int64_t seed) {
    g.setColour(Colours::black.withAlpha(0.5f));
    constexpr float dotDensity = 3.0f; // average spacing between dots
    int ix = roundToInt(x), iy = roundToInt(y);
    int iw = roundToInt(width), ih = roundToInt(height);
    if (iw < 2 || ih < 2) return;

    juce::Random rng(seed);
    constexpr int jitter = 1; // max random offset from grid position

    constexpr int dotSize = 2;
    for (float gx = 0; gx < (float)iw; gx += dotDensity) {
        for (float gy = 0; gy < (float)ih; gy += dotDensity) {
            int dx = jlimit(0, iw - dotSize, roundToInt(gx) + rng.nextInt(jitter * 2 + 1) - jitter);
            int dy = jlimit(0, ih - dotSize, roundToInt(gy) + rng.nextInt(jitter * 2 + 1) - jitter);
            g.fillRect(ix + dx, iy + dy, dotSize, dotSize);
        }
    }
}

/** Draw stripe pattern for envelope modulation based on envelope shape direction */
void drawEnvelopeStripes(Graphics& g, float x, float y, float width, float height, uint8_t shape) {
    g.setColour(Colours::black.withAlpha(0.5f));
    constexpr float nominalSpacing = 4.0f;
    constexpr float strokeWidth = 2.0f;

    // Determine direction based on envelope shape:
    // Up-first shapes: 4-7, C-F (attack first)
    // Down-first shapes: 0-3, 8-B (decay first)
    // Triangle shapes: A (down-up), E (up-down)
    bool isUpFirst = (shape >= 4 && shape <= 7) || (shape >= 0xC);
    bool isTriangle = (shape == 0xA || shape == 0xE);

    // For diagonal stripes, one period on x-axis = height (the line travels height pixels
    // horizontally). Fit a whole number of periods into the note width.
    // For zigzag, one V period = 2 * half-width on x.
    float periodX = isTriangle ? nominalSpacing * 2.0f : height;
    int numPeriods = jmax(1, roundToInt(width / periodX));
    float adjustedPeriod = width / (float)numPeriods;

    Path stripes;
    if (isTriangle) {
        float halfPeriod = adjustedPeriod * 0.5f;
        for (int p = 0; p < numPeriods; ++p) {
            float startX = x + (float)p * adjustedPeriod;
            // Up stroke
            stripes.startNewSubPath(startX, y + height);
            stripes.lineTo(startX + halfPeriod, y);
            // Down stroke
            stripes.lineTo(startX + adjustedPeriod, y + height);
        }
    } else if (isUpFirst) {
        // Forward stripes //// (attack) - one period = one diagonal line spanning height on x
        for (int p = 0; p < numPeriods; ++p) {
            float offset = (float)p * adjustedPeriod;
            stripes.startNewSubPath(x + offset, y + height);
            stripes.lineTo(x + offset + adjustedPeriod, y);
        }
    } else {
        // Backward stripes \\\\ (decay)
        for (int p = 0; p < numPeriods; ++p) {
            float offset = (float)p * adjustedPeriod;
            stripes.startNewSubPath(x + offset, y);
            stripes.lineTo(x + offset + adjustedPeriod, y + height);
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

    constexpr float pitchRangeInSemitones = 96.0f;

    te::TimePosition startPos = jmax(clipRange.getStart(), viewRange.getStart() - frameDur);
    te::TimePosition endPos = jmin(clipRange.getEnd(), viewRange.getEnd());

    const auto& frames = psgClip->getPsg().getFrames();
    if (frames.size() == 0)
        return;

    const auto startIdx = bisectFindPosition(frames, *psgClip, startPos);

    // First pass: find min/max normalized values across visible frames
    float minNorm = 1.0f;
    float maxNorm = 0.0f;
    for (int i = startIdx; i < frames.size(); ++i) {
        const auto& frame = frames[i];
        auto s = frame->getEditTime(*psgClip);
        if (s < startPos) continue;
        if (s >= endPos) break;

        const auto& frameData = frame->getData();
        for (int ch = 0; ch < 3; ++ch) {
            PsgParamType periodType (PsgParamType::TonePeriodA + ch);
            PsgParamType volumeType (PsgParamType::VolumeA + ch);
            PsgParamType toneOnType (PsgParamType::ToneIsOnA + ch);
            PsgParamType envOnType  (PsgParamType::EnvelopeIsOnA + ch);

            bool toneIsOn = frameData.getRaw(toneOnType) > 0;
            bool hasEnvMod = frameData.getRaw(envOnType) > 0;
            bool isAudible = (frameData.getRaw(volumeType) > 0) || hasEnvMod;

            if (toneIsOn && isAudible) {
                float pitch = periodType.valueToNormalized(frameData.getRaw(periodType));
                minNorm = jmin(minNorm, pitch);
                maxNorm = jmax(maxNorm, pitch);
            }
        }

        bool anyEnvMod = frameData.getRaw(PsgParamType::EnvelopeIsOnA) > 0 ||
                         frameData.getRaw(PsgParamType::EnvelopeIsOnB) > 0 ||
                         frameData.getRaw(PsgParamType::EnvelopeIsOnC) > 0;
        if (anyEnvMod) {
            PsgParamType envType(PsgParamType::EnvelopePeriod);
            float val = envType.valueToNormalized(frameData.getRaw(envType));
            minNorm = jmin(minNorm, val);
            maxNorm = jmax(maxNorm, val);
        }
    }

    // Handle edge cases and add padding
    if (minNorm > maxNorm) {
        // No values found - use full range
        minNorm = 0.0f;
        maxNorm = 1.0f;
    } else {
        // Pad one semitone on top and bottom
        float oneSemitone = 1.0f / pitchRangeInSemitones;
        float padding = oneSemitone;
        // Ensure minimum range of 2 semitones
        float minRange = 12.0f * oneSemitone;
        float range = maxNorm - minNorm;
        if (range < minRange) {
            float center = (minNorm + maxNorm) * 0.5f;
            minNorm = center - minRange * 0.5f;
            maxNorm = center + minRange * 0.5f;
        }
        // Extra top padding for legend (swatchSize + 2*pad ≈ 18px)
        float normRange = maxNorm - minNorm + 2.0f * padding;
        float legendPx = 18.0f;
        float topPadding = padding + normRange * legendPx / (float)rect.getHeight();
        float bottomPadding = 2.0f * oneSemitone;
        minNorm = jmax(0.0f, minNorm - bottomPadding);
        maxNorm = jmin(1.0f, maxNorm + topPadding);
    }

    const float normRange = maxNorm - minNorm;
    const float noteHeight = static_cast<float>(rect.getHeight()) / (normRange * pitchRangeInSemitones);

    paintNotes(g, *psgClip, rect, pixelsPerFrame, startIdx, startPos, endPos,
               noteHeight, maxNorm, normRange);
    paintLegend(g, *psgClip, rect, timeToX);
}

void PsgClipComponent::paintNotes(Graphics& g, PsgClip& psgClip, const juce::Rectangle<int>& rect,
                                   float pixelsPerFrame, int startIdx, te::TimePosition startPos,
                                   te::TimePosition endPos, float noteHeight, float maxNorm, float normRange) {
    const auto& frames = psgClip.getPsg().getFrames();

    auto normToY = [&](float norm) {
        return (maxNorm - norm) / normRange * rect.getHeight();
    };

    auto timeToX = [w = rect.getWidth(), s = psgClip.getEditTimeRange().getStart(),
                    len = psgClip.getEditTimeRange().getLength(), left = rect.getX()] (auto time) {
        return static_cast<float>(((time - s) * w) / len - left);
    };

    static const juce::Colour channelColors[] = {
        Colors::PSG::A,
        Colors::PSG::B,
        Colors::PSG::C,
    };

    for (int i = startIdx; i < frames.size(); ++i) {
        const auto& frame = frames[i];
        jassert(frame != nullptr);
        auto s = frame->getEditTime(psgClip);

        if (s < startPos)
            continue;
        if (s >= endPos)
            break;

        float x1 = timeToX(s);
        if (x1 + pixelsPerFrame < 0)
            continue;

        const auto& frameData = frame->getData();
        uint8_t envShape = static_cast<uint8_t>(frameData.getRaw(PsgParamType::EnvelopeShape));

        for (int ch = 0; ch < 3; ++ch) {
            PsgParamType periodType (PsgParamType::TonePeriodA + ch);
            PsgParamType volumeType (PsgParamType::VolumeA + ch);
            PsgParamType toneOnType (PsgParamType::ToneIsOnA + ch);
            PsgParamType envOnType  (PsgParamType::EnvelopeIsOnA + ch);
            PsgParamType noiseOnType(PsgParamType::NoiseIsOnA + ch);

            const auto rawVolume = frameData.getRaw(volumeType);
            bool toneIsOn = frameData.getRaw(toneOnType) > 0;
            bool hasEnvMod = frameData.getRaw(envOnType) > 0;
            bool hasNoiseMod = frameData.getRaw(noiseOnType) > 0;
            bool isAudible = (rawVolume > 0) || hasEnvMod;

            if (toneIsOn && isAudible) {
                auto period = frameData.getRaw(periodType);
                float pitch = periodType.valueToNormalized(period);
                float alpha = hasEnvMod ? 1.0f : (static_cast<float>(rawVolume) / 15.0f * 0.8f + 0.2f);
                float noteY = normToY(pitch) - noteHeight * 0.5f;

                g.setColour(channelColors[ch].withAlpha(alpha));
                g.fillRect(x1, noteY, pixelsPerFrame, noteHeight);

                if (hasEnvMod)
                    drawEnvelopeStripes(g, x1, noteY, pixelsPerFrame, noteHeight, envShape);

                if (hasNoiseMod)
                    drawNoisePattern(g, x1, noteY, pixelsPerFrame, noteHeight, (int64_t)i * 3 + ch);
            }
        }

        // Envelope period
        bool anyEnvMod = frameData.getRaw(PsgParamType::EnvelopeIsOnA) > 0 ||
                         frameData.getRaw(PsgParamType::EnvelopeIsOnB) > 0 ||
                         frameData.getRaw(PsgParamType::EnvelopeIsOnC) > 0;
        if (anyEnvMod) {
            PsgParamType envType(PsgParamType::EnvelopePeriod);
            float val = envType.valueToNormalized(frameData.getRaw(envType));
            float envY = normToY(val) - noteHeight * 0.5f;
            g.setColour(Colors::PSG::Env.withSaturation(1.0f).withAlpha(0.75f));
            g.fillRect(x1, envY, pixelsPerFrame, noteHeight);
            drawEnvelopeStripes(g, x1, envY, pixelsPerFrame, noteHeight, envShape);
        }
    }
}

void PsgClipComponent::paintLegend(Graphics& g, PsgClip& psgClip, const juce::Rectangle<int>& rect,
                                    std::function<float(te::TimePosition)> timeToX) {
    constexpr float pad = 3.0f;
    constexpr float swatchSize = 12.0f;
    constexpr float spacing = 1.0f;

    struct LegendItem { const char* label; Colour color; };
    const LegendItem items[] = {
        { "A", Colors::PSG::A },
        { "B", Colors::PSG::B },
        { "C", Colors::PSG::C },
        { "E", Colors::PSG::Env },
    };

    const auto viewRange = editViewState.zoom.getRange();
    g.setFont(Font(FontOptions(swatchSize - 1.0f).withStyle("Bold")));
    float visibleLeft = jmax((float)rect.getX(), timeToX(viewRange.getStart()));
    float x = visibleLeft + pad;
    float y = rect.getY() + pad;

    for (const auto& item : items) {
        g.setColour(item.color);
        g.fillRect(x, y, swatchSize, swatchSize);
        g.setColour(Colours::black);
        g.drawText(item.label, (int)x, (int)y, (int)swatchSize, (int)swatchSize, Justification::centred);
        x += swatchSize + spacing;
    }

    x += pad;
    g.setColour(Colours::white.withAlpha(0.7f));
    g.drawText(psgClip.getName(), (int)x, (int)y, rect.getWidth() - (int)x, (int)swatchSize, Justification::centredLeft);
}

}  // namespace MoTool
