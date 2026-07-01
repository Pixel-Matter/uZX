#include <JuceHeader.h>

#include "PsgClipComponent.h"
#include "../common/LookAndFeel.h"
#include "../../models/EditUtilities.h"
#include "../../models/Ids.h"

#include <algorithm>
#include <tuple>

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

struct FrameNote {
    float y;
    float height;
    float alpha;
    int   channelIndex;  // 0=A, 1=B, 2=C, 3=Envelope
    bool  hasEnvMod;
    bool  hasNoiseMod;
};

/** Frame duration in edit time, taken from the clip's own PSG timing metadata.
    Falls back to the stored source rate and finally the edit's project fps for
    legacy clips, mirroring the fallback chain in getFrameBeatPosition(). */
te::TimeDuration getClipFrameDuration(const PsgClip& clip) {
    double fps = clip.getPsg().getEffectiveFps(clip);
    if (fps <= 0.0)
        fps = clip.getPsg().getFrameRate();
    if (fps <= 0.0)
        fps = Helpers::getProjectFps(clip.edit);
    return te::TimeDuration::fromSeconds(1.0 / fps);
}

/** The frame-to-pixel mapping shared by the notes and registers painters. */
struct ClipVisibility {
    Rectangle<int> rect;
    te::TimeRange clipRange;
    te::TimeDuration frameDur;
    te::TimeRange range;   // visible part of the clip, with one frame of slack on the left
    int startIdx;          // first frame that can fall inside range
    float pixelsPerFrame;

    ClipVisibility(const PsgClip& clip,
                   const juce::Array<PsgParamFrame*>& frames,
                   const EditViewState& evs,
                   Rectangle<int> rectangle)
        : rect(rectangle)
        , clipRange(clip.getEditTimeRange())
        , frameDur(getClipFrameDuration(clip))
        , range(jmax(clipRange.getStart(), evs.zoom.getRange().getStart() - frameDur),
                jmin(clipRange.getEnd(), evs.zoom.getRange().getEnd()))
        , startIdx(bisectFindPosition(frames, clip, range.getStart()))
        , pixelsPerFrame(static_cast<float>(frameDur.inSeconds() * rect.getWidth()
                                            / clipRange.getLength().inSeconds()))
    {
    }

    float timeToX(te::TimePosition time) const {
        return static_cast<float>(((time - clipRange.getStart()) * rect.getWidth()) / clipRange.getLength());
    }

    /** Calls fn(frameArrayIndex, frame, x) for every frame inside the visible range. */
    template <typename Fn>
    void forEachVisibleFrame(const juce::Array<PsgParamFrame*>& frames, const PsgClip& clip, Fn&& fn) const {
        for (int i = startIdx; i < frames.size(); ++i) {
            const auto& frame = *frames[i];
            const auto t = frame.getEditTime(clip);

            if (t < range.getStart())
                continue;
            if (t >= range.getEnd())
                break;

            fn(i, frame, timeToX(t));
        }
    }
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

void PsgClipComponent::valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier& property) {
    // The header's effective-fps warning depends on frames-per-beat, current beat length
    // and the edit's timecode fps, so refresh when tempo or timecode changes.
    if (property == te::IDs::timecodeFormat
        || property == te::IDs::bpm
        || property == te::IDs::startBeat)
        repaint();
}

void PsgClipComponent::paint(Graphics& g) {
    ClipComponent::paint(g);

    GUIPaintMeasurer::ScopedTimer timer(paintMeasurer_);

    int mode = clip->state.getProperty(IDs::paintMode, 0);
    if (mode == 0)
        paintNotes(g);
    else
        paintRegisters(g);

    paintHeader(g);

    paintMeasurer_.drawOverlay(g);
}

void PsgClipComponent::mouseDown(const MouseEvent& e) {
    if (e.mods.isPopupMenu()) {
        auto* psgClip = getPsgClip();
        if (psgClip == nullptr) return;

        int mode = clip->state.getProperty(IDs::paintMode, 0);

        PopupMenu m;
        m.addItem("Parameters", true, mode == 0, [this] {
            clip->state.setProperty(IDs::paintMode, 0, nullptr);
            repaint();
        });
        m.addItem("Registers", true, mode == 1, [this] {
            clip->state.setProperty(IDs::paintMode, 1, nullptr);
            repaint();
        });
        m.showMenuAsync({});
    } else {
        ClipComponent::mouseDown(e);
    }
}

void PsgClipComponent::paintRegisters(Graphics& g) {
    auto* psgClip = getPsgClip();
    if (psgClip == nullptr) return;

    const auto& frames = psgClip->getPsg().getFrames();
    if (frames.isEmpty())
        return;

    const ClipVisibility vis {*psgClip, frames, editViewState, getLocalBounds()};

    constexpr auto numRegs = uZX::PsgRegsFrame::size();
    const auto height = static_cast<float>(vis.rect.getHeight());
    const float laneHeight = std::round(height / numRegs);
    const bool showHexValues = vis.pixelsPerFrame >= 12.0f;

    g.setFont(12.0f);

    uZX::PsgRegsFrame regsFrame;
    vis.forEachVisibleFrame(frames, *psgClip, [&] (int, const PsgParamFrame& frame, float x) {
        regsFrame.clear();
        frame.getData().updateRegisters(regsFrame);

        for (size_t reg = 0; reg < regsFrame.size(); ++reg) {
            if (! regsFrame.isSet(reg))
                continue;

            const auto value = regsFrame.getRaw(reg);
            const float y = static_cast<float>(reg) / numRegs * height;
            const auto color = RegColors[reg];

            if (showHexValues) {
                String hexValue = choc::text::createHexString(value, 2);
                g.setColour(color.withLightness(0.75f));
                g.fillRect(x, y, vis.pixelsPerFrame, laneHeight);
                g.setColour(Colours::black);
                g.drawText(hexValue, (int)(x + 1), (int)y, (int)vis.pixelsPerFrame, (int)laneHeight, Justification::centredLeft);
            } else {
                const auto brightness = static_cast<float>(value) / 255.0f;
                g.setColour(color.withLightness(0.75f).withAlpha(0.5f + brightness / 2.0f));
                g.fillRect(x, y, vis.pixelsPerFrame, laneHeight);
            }
        }
    });
}

void PsgClipComponent::paintNotes(Graphics& g) {
    auto* psgClip = getPsgClip();
    if (psgClip == nullptr)
        return;

    const auto& frames = psgClip->getPsg().getFrames();
    if (frames.isEmpty())
        return;

    const ClipVisibility vis {*psgClip, frames, editViewState, getLocalBounds()};

    const auto pitchRange = psgClip->getPitchRange();
    const float noteHeight = static_cast<float>(vis.rect.getHeight())
        / (pitchRange.getLength() * PsgParamType{PsgParamType::TonePeriodA}.getScale().octaves() * 12.0f);
    const bool drawMods = vis.pixelsPerFrame >= 6.0f && noteHeight >= 2.0f;

    auto normToY = [&] (float norm) {
        return (pitchRange.getEnd() - norm) / pitchRange.getLength() * static_cast<float>(vis.rect.getHeight());
    };

    static const juce::Colour channelColors[] = {
        Colors::PSG::A,
        Colors::PSG::B,
        Colors::PSG::C,
        Colors::PSG::Env,
    };

    vis.forEachVisibleFrame(frames, *psgClip, [&] (int frameIdx, const PsgParamFrame& frame, float x) {
        const auto& frameData = frame.getData();
        const auto envShape = static_cast<uint8_t>(frameData.getRaw(PsgParamType::EnvelopeShape));

        // Collect this frame's notes: up to one per channel plus the envelope
        std::array<FrameNote, 4> notes;
        int noteCount = 0;

        visitFrameNotes(frameData, [&] (const PsgFrameNote& n) {
            const float alpha = n.hasEnvMod ? 1.0f : (static_cast<float>(n.volume) / 15.0f * 0.8f + 0.2f);
            const float top = normToY(n.pitch) - noteHeight * 0.5f;
            const float topRound = static_cast<float>(roundToInt(top));
            const float heightRound = static_cast<float>(roundToInt(top + noteHeight)) - topRound;
            notes[noteCount++] = { topRound, heightRound, alpha, n.channelIndex, n.hasEnvMod, n.hasNoiseMod };
        });

        std::sort(notes.begin(), notes.begin() + noteCount, [] (const FrameNote& a, const FrameNote& b) {
            return std::tie(a.y, a.channelIndex) < std::tie(b.y, b.channelIndex);
        });

        // Paint notes, subdividing overlapping groups. Notes are sorted by y,
        // so a group extends as long as the next note's top is within the
        // current group's bottom.
        int gi = 0;
        while (gi < noteCount) {
            const int groupStart = gi;
            const float groupY = notes[gi].y;
            float groupBottom = notes[gi].y + notes[gi].height;
            ++gi;
            while (gi < noteCount && notes[gi].y < groupBottom) {
                groupBottom = jmax(groupBottom, notes[gi].y + notes[gi].height);
                ++gi;
            }
            const int groupSize = gi - groupStart;
            const float groupH = groupBottom - groupY;

            // Graceful degradation: if sub-lane height < 1px, paint full height
            const bool subdivide = groupSize > 1 && groupH / static_cast<float>(groupSize) >= 1.0f;

            for (int j = 0; j < groupSize; ++j) {
                const auto& note = notes[groupStart + j];
                float subY = note.y, subH = note.height;
                if (subdivide) {
                    subH = std::floor(groupH / static_cast<float>(groupSize));
                    subY = groupY + static_cast<float>(j) * subH;
                    // Last sub-lane absorbs any rounding remainder
                    if (j == groupSize - 1)
                        subH = groupBottom - subY;
                }

                g.setColour(channelColors[note.channelIndex].withAlpha(note.alpha));
                g.fillRect(x, subY, vis.pixelsPerFrame, subH);

                if (drawMods && note.hasEnvMod)
                    drawEnvelopeStripes(g, x, subY, vis.pixelsPerFrame, subH, envShape);

                if (drawMods && note.hasNoiseMod)
                    drawNoisePattern(g, x, subY, vis.pixelsPerFrame, subH,
                                     (int64_t)frameIdx * 4 + note.channelIndex);
            }
        }
    });
}

void PsgClipComponent::paintHeader(Graphics& g) {
    auto* psgClip = getPsgClip();
    if (psgClip == nullptr) return;

    constexpr int pad = 3;
    constexpr int swatchSize = 12;
    constexpr int spacing = 1;

    // The clip component can extend far off-screen (its width is the whole clip in
    // pixels), so anchor the header to the currently-visible slice rather than the
    // component's own left edge. getClipBounds() is that slice, in local coords.
    auto row = g.getClipBounds().removeFromTop(headerHeight).reduced(pad, pad);

    // Channel swatches on the left.
    struct LegendItem { const char* label; Colour color; };
    const LegendItem items[] = {
        { "A", Colors::PSG::A },
        { "B", Colors::PSG::B },
        { "C", Colors::PSG::C },
        { "E", Colors::PSG::Env },
    };

    g.setFont(Font(FontOptions((float) swatchSize - 1.0f).withStyle("Bold")));
    for (const auto& item : items) {
        auto swatch = row.removeFromLeft(swatchSize);
        g.setColour(item.color);
        g.fillRect(swatch);
        g.setColour(Colours::black);
        g.drawText(item.label, swatch, Justification::centred);
        row.removeFromLeft(spacing);
    }
    row.removeFromLeft(pad);

    // fps warning chip after the swatches, only when the clip's fps differs from the
    // edit's. Kept on the left so it never collides with the paint-measurer overlay,
    // which draws in the top-right corner.
    const auto effectiveFps = psgClip->getPsg().getEffectiveFps(*psgClip);
    const auto editFps = Helpers::getProjectFps(psgClip->edit);

    if (psgClip->getPsg().hasFpsMismatch(*psgClip, editFps)) {
        auto text = String::fromUTF8("\xE2\x9A\xA0 ")  // ⚠
                  + String(roundToInt(effectiveFps)) + " @ "
                  + String(roundToInt(editFps)) + " fps";

        g.setFont(Font(FontOptions((float) swatchSize - 1.0f)));
        const int chipWidth = jmin(row.getWidth(), GlyphArrangement::getStringWidthInt(g.getCurrentFont(), text) + 2 * pad);
        auto chip = row.removeFromLeft(chipWidth);

        g.setColour(Colors::Theme::warning.withAlpha(0.9f));
        g.fillRoundedRectangle(chip.toFloat(), 2.0f);
        g.setColour(Colours::black);
        g.drawText(text, chip.reduced(pad, 0), Justification::centred, true);

        row.removeFromLeft(pad);
    }

    // Clip name fills whatever space is left.
    g.setFont(Font(FontOptions((float) swatchSize - 1.0f)));
    g.setColour(Colours::white.withAlpha(0.7f));
    g.drawText(psgClip->getName(), row, Justification::centredLeft, true);
}

}  // namespace MoTool
