#include <JuceHeader.h>

#include "PsgClipComponent.h"
#include "../common/LookAndFeel.h"
#include "../../models/EditUtilities.h"

namespace MoTool {

namespace {

static juce::Range<float> findVisiblePitchRange(
    const juce::Array<PsgParamFrame*>& frames,
    const PsgClip& psgClip,
    int startIdx,
    te::TimeRange visibleRange,
    float rectHeight)
{
    float min = 1.0f;
    float max = 0.0f;
    for (int i = startIdx; i < frames.size(); ++i) {
        const auto& frame = frames[i];
        auto s = frame->getEditTime(psgClip);
        if (s < visibleRange.getStart()) continue;
        if (s >= visibleRange.getEnd()) break;

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
                min = jmin(min, pitch);
                max = jmax(max, pitch);
            }
        }

        bool anyEnvMod = frameData.getRaw(PsgParamType::EnvelopeIsOnA) > 0 ||
                         frameData.getRaw(PsgParamType::EnvelopeIsOnB) > 0 ||
                         frameData.getRaw(PsgParamType::EnvelopeIsOnC) > 0;
        if (anyEnvMod) {
            PsgParamType envType(PsgParamType::EnvelopePeriod);
            float val = envType.valueToNormalized(frameData.getRaw(envType));
            min = jmin(min, val);
            max = jmax(max, val);
        }
    }

    if (min > max) {
        min = 0.0f;
        max = 1.0f;
    } else {
        const float oneSemitone = 1.f / PsgParamType{PsgParamType::TonePeriodA}.getScale().octaves() / 12.f;
        const float minRange = 12.f * oneSemitone;
        if (max - min < minRange) {
            float center = (min + max) / 2.f;
            min = center - minRange * 0.5f;
            max = center + minRange * 0.5f;
        }
        const float padding = oneSemitone * 1.5f;
        float normRange = max - min + 2.f * padding;
        min = jmax(0.0f, min - padding);
        max = jmin(1.0f, max + padding);
    }
    return { min, max };
}

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

struct FrameNote {
    float noteYround;
    float heightRound;
    float alpha;
    int   channelIndex;  // 0=A, 1=B, 2=C, 3=Envelope
    bool  hasEnvMod;
    bool  hasNoiseMod;
};

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
    paintNotes(g);
    paintLegend(g);
}

struct ClipVisibility {
    Rectangle<int> rect;
    te::TimeRange clipRange;
    te::TimeDuration frameDur;
    te::TimeRange range;
    int startIdx;
    juce::Range<float> pitchRange;
    float pixelsPerFrame;
    float noteHeight;

    ClipVisibility(const PsgClip& clip,
                   const juce::Array<PsgParamFrame*>& frames,
                   const EditViewState& evs,
                   Rectangle<int> rectangle)
        : rect(rectangle)
        , clipRange(clip.getEditTimeRange())
        , frameDur(te::TimeDuration::fromSeconds(1.0f / Helpers::getEditTimecodeFormat(clip.edit).getFPS()))
        , range(jmax(clipRange.getStart(), evs.zoom.getRange().getStart() - frameDur),
                jmin(clipRange.getEnd(), evs.zoom.getRange().getEnd()))
        , startIdx(bisectFindPosition(frames, clip, range.getStart()))
        , pitchRange(findVisiblePitchRange(frames, clip, startIdx, range, static_cast<float>(rect.getHeight())))
        , pixelsPerFrame(static_cast<float>(frameDur.inSeconds() * rect.getWidth())
                         / static_cast<float>(clipRange.getLength().inSeconds()))
        , noteHeight(rect.getHeight()
                     / (pitchRange.getLength() * PsgParamType{PsgParamType::TonePeriodA}.getScale().octaves() * 12.f))
    {
    }

    inline float normToY(float norm) const {
        return (pitchRange.getEnd() - norm) / pitchRange.getLength() * static_cast<float>(rect.getHeight());
    }

    inline float timeToX(te::TimePosition time) const {
        return static_cast<float>(((time - clipRange.getStart()) * rect.getWidth()) / clipRange.getLength()
                                  - rect.getX());
    }
};

void PsgClipComponent::paintNotes(Graphics& g) {
    auto* psgClip = getPsgClip();
    if (psgClip == nullptr)
        return;

    const auto rect = getLocalBounds();
    const auto& frames = psgClip->getPsg().getFrames();
    const auto vis = ClipVisibility {*psgClip, frames, editViewState, rect};
    const bool drawMods = vis.pixelsPerFrame >= 6.0f && vis.noteHeight >= 2.0f;

    static const juce::Colour channelColors[] = {
        Colors::PSG::A,
        Colors::PSG::B,
        Colors::PSG::C,
        Colors::PSG::Env,
    };

    for (int i = vis.startIdx; i < frames.size(); ++i) {
        const auto& frame = frames[i];
        jassert(frame != nullptr);
        auto s = frame->getEditTime(*psgClip);

        if (s < vis.range.getStart())
            continue;
        if (s >= vis.range.getEnd())
            break;

        float x1 = vis.timeToX(s);
        if (x1 + vis.pixelsPerFrame < 0)
            continue;

        const auto& frameData = frame->getData();
        uint8_t envShape = static_cast<uint8_t>(frameData.getRaw(PsgParamType::EnvelopeShape));

        // Collect all visible notes for this frame
        std::array<FrameNote, 4> notes;
        int noteCount = 0;

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
                float noteY = vis.normToY(pitch) - vis.noteHeight * 0.5f;
                float noteYround = static_cast<float>(roundToInt(noteY));
                float heightRound = static_cast<float>(roundToInt(noteY + vis.noteHeight)) - noteYround;

                notes[noteCount++] = { noteYround, heightRound, alpha, ch, hasEnvMod, hasNoiseMod };
            }
        }

        // Envelope period note
        bool anyEnvMod = frameData.getRaw(PsgParamType::EnvelopeIsOnA) > 0 ||
                         frameData.getRaw(PsgParamType::EnvelopeIsOnB) > 0 ||
                         frameData.getRaw(PsgParamType::EnvelopeIsOnC) > 0;
        if (anyEnvMod) {
            PsgParamType envType(PsgParamType::EnvelopePeriod);
            float val = envType.valueToNormalized(frameData.getRaw(envType));
            float envY = vis.normToY(val) - vis.noteHeight * 0.5f;
            float envYround = static_cast<float>(roundToInt(envY));
            float envHeightRound = static_cast<float>(roundToInt(envY + vis.noteHeight)) - envYround;

            notes[noteCount++] = { envYround, envHeightRound, 1.0f, 3, true, false };
        }

        // Sort by noteYround then channelIndex (insertion sort for <= 4 elements)
        for (int a = 1; a < noteCount; ++a) {
            auto key = notes[a];
            int b = a - 1;
            while (b >= 0 && (notes[b].noteYround > key.noteYround ||
                              (notes[b].noteYround == key.noteYround && notes[b].channelIndex > key.channelIndex))) {
                notes[b + 1] = notes[b];
                --b;
            }
            notes[b + 1] = key;
        }

        // Paint notes, subdividing overlapping groups
        int gi = 0;
        while (gi < noteCount) {
            // Find group of notes whose vertical rects overlap.
            // Notes are sorted by noteYround, so we extend the group as long as
            // the next note's top is within the current group's bottom.
            int groupStart = gi;
            float groupY = notes[gi].noteYround;
            float groupBottom = notes[gi].noteYround + notes[gi].heightRound;
            ++gi;
            while (gi < noteCount && notes[gi].noteYround < groupBottom) {
                groupBottom = jmax(groupBottom, notes[gi].noteYround + notes[gi].heightRound);
                ++gi;
            }
            int groupSize = gi - groupStart;
            float groupH = groupBottom - groupY;

            // Graceful degradation: if sub-lane height < 1px, paint full height
            bool subdivide = groupSize > 1 && groupH / static_cast<float>(groupSize) >= 1.0f;

            for (int j = 0; j < groupSize; ++j) {
                const auto& note = notes[groupStart + j];
                float subY, subH;
                if (subdivide) {
                    subH = std::floor(groupH / static_cast<float>(groupSize));
                    subY = groupY + static_cast<float>(j) * subH;
                    // Last sub-lane absorbs any rounding remainder
                    if (j == groupSize - 1)
                        subH = groupBottom - subY;
                } else {
                    subY = note.noteYround;
                    subH = note.heightRound;
                }

                g.setColour(channelColors[note.channelIndex].withAlpha(note.alpha));
                g.fillRect(x1, subY, vis.pixelsPerFrame, subH);

                if (note.hasEnvMod && drawMods)
                    drawEnvelopeStripes(g, x1, subY, vis.pixelsPerFrame, subH, envShape);

                if (note.hasNoiseMod && drawMods)
                    drawNoisePattern(g, x1, subY, vis.pixelsPerFrame, subH,
                                     (int64_t)i * 4 + note.channelIndex);
            }
        }
    }
}

void PsgClipComponent::paintLegend(Graphics& g) {

    auto* psgClip = getPsgClip();
    if (psgClip == nullptr) return;

    const auto rect = getLocalBounds();
    const auto viewRange = editViewState.zoom.getRange();
    const auto clipRange = psgClip->getEditTimeRange();
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

    g.setFont(Font(FontOptions(swatchSize - 1.0f).withStyle("Bold")));

    float visibleLeft =
        jmax((float) rect.getX(),
             (float) (((viewRange.getStart() - clipRange.getStart()) * rect.getWidth()) / clipRange.getLength() - rect.getX()));

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
    g.drawText(psgClip->getName(), (int)x, (int)y, rect.getWidth() - (int)x, (int)swatchSize, Justification::centredLeft);
}

}  // namespace MoTool
