#include <JuceHeader.h>

#include "PsgClipComponent.h"
#include "../common/LookAndFeel.h"
#include "../../models/EditUtilities.h"
#include "../../models/Ids.h"
#include "../../plugins/uZX/aychip/AYPlugin.h"

#include <algorithm>
#include <cmath>
#include <span>
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

struct FrameNote {
    float y;
    float height;
    float alpha;
    uint8_t channelMask;
    bool isEnvelopePitch;
    bool hasEnvMod;
};

Colour getChannelMaskColor(uint8_t channelMask) {
    switch (channelMask & 0x7u) {
        case 0x1: return Colors::PSG::A;
        case 0x2: return Colors::PSG::B;
        case 0x3: return Colors::Palette::yellow500.withSaturation(1.0f);
        case 0x4: return Colors::PSG::C;
        case 0x5: return Colors::Palette::pink500.withSaturation(1.0f);
        case 0x6: return Colors::Palette::cyan500.withSaturation(1.0f);
        case 0x7: return Colors::PSG::Mix.brighter(0.8f);
        default:  return Colours::transparentBlack;
    }
}

float noisePeriodToYNorm(int noisePeriod) {
    auto y = std::log2(static_cast<float>(jlimit(1, 31, noisePeriod))) / std::log2(31.0f);
    return jmap(y, 1.0f/16, 1.0f - 1.0f/16);
}

Range<float> getNoisePeriodYRange(int noisePeriod) {
    const float Y = noisePeriodToYNorm(noisePeriod);
    return { Y - 0.1f, Y + 0.1f };
}

std::array<float, 3> getNoiseChannelLevels(const PsgParamFrameData& data) {
    std::array<float, 3> levels {};

    for (int ch = 0; ch < 3; ++ch) {
        const bool hasNoiseMod = data.getRaw(PsgParamType(PsgParamType::NoiseIsOnA + ch)) > 0;
        if (! hasNoiseMod)
            continue;

        const auto rawVolume = data.getRaw(PsgParamType(PsgParamType::VolumeA + ch));
        const bool hasEnvMod = data.getRaw(PsgParamType(PsgParamType::EnvelopeIsOnA + ch)) > 0;
        levels[static_cast<size_t>(ch)] = hasEnvMod ? 1.0f : static_cast<float>(rawVolume) / 15.0f;
    }

    return levels;
}

void drawNoiseBand(Graphics& g, Rectangle<float> bounds, float alpha) {
    if (bounds.isEmpty() || alpha <= 0.0f)
        return;
    const auto color = Colors::PSG::Mix;
    ColourGradient gradient(color.withAlpha(0.0f), bounds.getX(), bounds.getY(),
                            color.withAlpha(0.0f), bounds.getX(), bounds.getBottom(), false);
    gradient.addColour(0.5, color.withAlpha(alpha));
    g.setGradientFill(gradient);
    g.fillRect(bounds);
}

void drawNoiseFringe(Graphics& g, Rectangle<float> bounds, int spread, uint8_t channelMask,
                     const std::array<float, 3>& levels, int frameIndex, Rectangle<float> viewport,
                     std::span<const FrameNote> notes) {
    std::array<size_t, 3> channels {};
    int channelCount = 0;
    for (size_t ch = 0; ch < levels.size(); ++ch)
        if ((channelMask & (1u << ch)) != 0 && levels[ch] > 0.0f)
            channels[static_cast<size_t>(channelCount++)] = ch;
    if (channelCount == 0 || spread < 2)
        return;

    const int minX = static_cast<int>(std::ceil(jmax(bounds.getX(), viewport.getX())));
    const int maxX = static_cast<int>(std::floor(jmin(bounds.getRight(), viewport.getRight())));
    const int minY = static_cast<int>(std::ceil(jmax(bounds.getY() - float(spread), viewport.getY())));
    const int maxY = static_cast<int>(std::floor(jmin(bounds.getBottom() + float(spread), viewport.getBottom())));
    const auto clip = g.getClipBounds();
    const int left = static_cast<int>(std::ceil(bounds.getX()));
    const int firstColumn = jmax(0, (jmax(minX, clip.getX()) - left) / 6);
    const int lastColumn = jmax(0, (jmin(maxX, clip.getRight()) - left + 5) / 6);
    constexpr std::array<uint8_t, 16> pattern {19, 52, 7, 41, 28, 60, 3, 46, 35, 12, 58, 23, 49, 6, 31, 44};
    const auto phase = static_cast<uint32_t>(frameIndex) * 5u + channelMask;
    auto drawMark = [&](int x, int y) {
        // Test geometry against content bounds; damage clipping must not change the pattern.
        if (x < minX || x + 2 > maxX || y < minY || y + 2 > maxY)
            return;
        for (const auto& note : notes)
            if (float(y) < note.y + note.height && float(y + 2) > note.y)
                return;
        g.fillRect(x, y, 2, 2);
    };

    // Fixed rectangular stipple: no RNG, allocation, or gradient per dot; visit only visible columns.
    for (int owner = 0; owner < channelCount; ++owner) {
        const auto ch = channels[static_cast<size_t>(owner)];
        g.setColour(getChannelMaskColor(static_cast<uint8_t>(1u << ch)).withAlpha(0.25f + 0.5f * levels[ch]));
        for (int depth = 0; depth + 2 <= spread; depth += 3) {
            const int first = firstColumn + (owner + channelCount - firstColumn % channelCount) % channelCount;
            for (int column = first; column < lastColumn; column += channelCount) {
                const auto bits = pattern[(static_cast<uint32_t>(column + depth * 7) + phase) & 15u];
                const int upperOffset = jmin(depth + ((bits >> 4) & 1), spread - 2);
                const int lowerOffset = jmin(depth + ((bits >> 5) & 1), spread - 2);
                drawMark(left + column * 6 + (bits & 3), roundToInt(bounds.getY()) - upperOffset - 2);
                drawMark(left + column * 6 + ((bits >> 2) & 3), roundToInt(bounds.getBottom()) + lowerOffset);
            }
        }
    }
}

/** Frame duration in edit time, taken from the clip's own PSG timing metadata. */
te::TimeDuration getClipFrameDuration(const PsgClip& clip) {
    double fps = clip.getPsg().getEffectiveFps(clip);
    if (fps <= 0.0) {
        jassertfalse;  // PsgClip::initialise guarantees framesPerBeat > 0
        fps = Helpers::getProjectFps(clip.edit);
    }
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
void drawEnvelopeStripes(Graphics& g, float x, float y, float width, float height, uint8_t shape, Colour color) {
    g.setColour(color);
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

void drawLowEnvelopeArrow(Graphics& g, Rectangle<float> bounds, uint8_t owners, bool drawDetails) {
    const auto color = Colors::PSG::Env;
    if (bounds.getHeight() < 4.0f || bounds.getWidth() < 4.0f) {
        g.setColour(color);
        g.fillRect(bounds);
        return;
    }
    bounds = bounds.reduced(0.75f);
    const float middle = bounds.getCentreX();
    const float shoulderY = bounds.getY() + bounds.getHeight() * 0.45f;
    const float stemLeft = middle - bounds.getWidth() * 0.2f;
    const float stemRight = middle + bounds.getWidth() * 0.2f;
    Path arrow;
    arrow.startNewSubPath(stemLeft, bounds.getY());
    arrow.lineTo(stemRight, bounds.getY());
    arrow.lineTo(stemRight, shoulderY);
    arrow.lineTo(bounds.getRight(), shoulderY);
    arrow.lineTo(middle, bounds.getBottom());
    arrow.lineTo(bounds.getX(), shoulderY);
    arrow.lineTo(stemLeft, shoulderY);
    arrow.closeSubPath();
    g.setColour(color.withAlpha(0.65f));
    g.fillPath(arrow);
    if (drawDetails) {
        g.saveState();
        g.reduceClipRegion(arrow);
        drawEnvelopeStripes(g, bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(), 8, color);
        drawEnvelopeStripes(g, bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(), 12, color);
        g.restoreState();
    }
    g.setColour(getChannelMaskColor(owners));
    g.strokePath(arrow, PathStrokeType(1.5f));
}

} // namespace

std::vector<std::pair<String, Colour>> formatPsgFrame(const PsgParamFrameData& data, double chipClockHz) {
    using T = PsgParamType;
    std::vector<std::pair<String, Colour>> runs;
    Colour rowColour;
    const auto add = [&] (const String& text, bool changed = false) {
        runs.emplace_back(text, changed ? rowColour : rowColour.withMultipliedAlpha(0.5f));
    };
    const auto hz = [&] (int tonePeriod) {
        return String(chipClockHz / 16.0 / jmax(1, tonePeriod), 1) + "Hz";  // AY tone clock divider
    };

    // Rows A, B, C, Env, Noise share the period and Hz columns.
    const int envPeriod = data.getRaw(T::EnvelopePeriod);
    std::array<String, 5> periods, freqs, volumes;
    for (size_t ch = 0; ch < 3; ++ch) {
        const int period = data.getRaw(T(T::TonePeriodA + int(ch)));
        periods[ch] = String(period);
        freqs[ch] = hz(period);
        volumes[ch] = String(data.getRaw(T(T::VolumeA + int(ch))));
    }
    periods[3] = String(envPeriod);
    freqs[3] = hz(envelopePeriodToTonePeriod(jmax(1, envPeriod)));
    periods[4] = String(data.getRaw(T::NoisePeriod));
    const auto widest = [] (const std::array<String, 5>& column) {
        int width = 0;
        for (const auto& text : column)
            width = jmax(width, text.length());
        return width;
    };
    const int periodWidth = widest(periods), hzWidth = widest(freqs), volumeWidth = widest(volumes);
    const auto cell = [&] (const String& text, int width, bool changed) {
        add("  ");
        add(text.paddedLeft(' ', width), changed);
    };
    const auto label = [&] (const String& text, bool changed) { add(text.paddedRight(' ', 5), changed); };

    for (size_t ch = 0; ch < 3; ++ch) {
        const T period(T::TonePeriodA + int(ch)), volume(T::VolumeA + int(ch));
        const std::array<T, 3> flags {T(T::ToneIsOnA + int(ch)), T(T::NoiseIsOnA + int(ch)), T(T::EnvelopeIsOnA + int(ch))};
        bool rowChanged = data.isSet(period) || data.isSet(volume);
        for (auto flag : flags)
            rowChanged = rowChanged || data.isSet(flag);

        rowColour = getChannelMaskColor(static_cast<uint8_t>(1u << ch));
        label(String::charToString(juce_wchar('A' + ch)), rowChanged);
        cell(periods[ch], periodWidth, data.isSet(period));
        cell(freqs[ch], hzWidth, data.isSet(period));
        cell(volumes[ch], volumeWidth, data.isSet(volume));
        add("  ");
        for (size_t i = 0; i < 3; ++i)
            add(data.getRaw(flags[i]) ? String::charToString(juce_wchar("TNE"[i])) : String("-"), data.isSet(flags[i]));
        add("\n");
    }

    const bool shapeChanged = data.isSet(T::EnvelopeShape), envPeriodChanged = data.isSet(T::EnvelopePeriod);
    rowColour = Colors::PSG::Env;
    label("Env", shapeChanged || envPeriodChanged);
    cell(periods[3], periodWidth, envPeriodChanged);
    cell(freqs[3], hzWidth, envPeriodChanged);
    add("  shape ");
    add(String::toHexString(data.getRaw(T::EnvelopeShape)).toUpperCase(), shapeChanged);
    add("\n");

    rowColour = Colors::Theme::textPrimary;
    label("Noise", data.isSet(T::NoisePeriod));
    cell(periods[4], periodWidth, data.isSet(T::NoisePeriod));
    return runs;
}

//==============================================================================
/** Tooltip-styled popup showing formatPsgFrame() runs. */
class PsgClipComponent::FrameHover : public Component {
public:
    FrameHover() {
        setInterceptsMouseClicks(false, false);
        setAlwaysOnTop(true);
    }

    void setText(const std::vector<std::pair<String, Colour>>& runs) {
        const Font font(FontOptions(Font::getDefaultMonospacedFontName(), 12.0f, Font::plain));
        AttributedString text;
        for (const auto& [run, colour] : runs)
            text.append(run, font, colour);
        layout_.createLayout(text, 1.0e6f);
        setSize(roundToInt(layout_.getWidth()) + 2 * pad, roundToInt(layout_.getHeight()) + 2 * pad);
        repaint();
    }

    void paint(Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour(Colors::Theme::backgroundAlt);
        g.fillRoundedRectangle(bounds, 5.0f);
        g.setColour(Colors::Theme::border);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, 1.0f);
        layout_.draw(g, bounds.reduced(pad));
    }

private:
    static constexpr int pad = 6;
    TextLayout layout_;
};

//==============================================================================
PsgClipComponent::PsgClipComponent(EditViewState& evs, TimelineGrid& g, te::Clip::Ptr c)
    : MidiClipComponent(evs, g, c)
    // , vblankAttachment_(this, [this](double) { onVBlank(); })
{
    // setBufferedToImage(true);
    // The fps-warning in the header depends on the edit's timecode format,
    // so repaint when it changes.
    editViewState.edit.state.addListener(this);
}

PsgClipComponent::~PsgClipComponent() {
    editViewState.edit.state.removeListener(this);
}

PsgClip* PsgClipComponent::getPsgClip() {
    return dynamic_cast<PsgClip*>(clip.get());
}

void PsgClipComponent::valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier& property) {
    // The header's effective-fps warning depends on frames-per-beat, current beat length
    // and the edit's timecode fps, so refresh when tempo or timecode changes.
    if (property == IDs::timecodeDisplayMode
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
    hideFrameHover();
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

void PsgClipComponent::mouseMove(const MouseEvent&) {
    hideFrameHover();
    startTimer(2000);
}

void PsgClipComponent::mouseExit(const MouseEvent&) {
    hideFrameHover();
}

void PsgClipComponent::timerCallback() {
    stopTimer();
    showFrameHover();
}

std::vector<std::pair<String, Colour>> PsgClipComponent::describeFrameAt(int x) {
    auto* psgClip = getPsgClip();
    if (psgClip == nullptr)
        return {};

    // The frame sounding at time is the last one starting at or before it.
    const auto& frames = psgClip->getPsg().getFrames();
    const auto time = editViewState.zoom.xToTime(x + getX());
    int i = bisectFindPosition(frames, *psgClip, time);
    if (i == frames.size() || frames[i]->getEditTime(*psgClip) > time)
        --i;
    if (i < 0)
        return {};

    // Past the frame's own slot its state just holds, so nothing changes there.
    auto data = frames[i]->getData();
    if (time >= frames[i]->getEditTime(*psgClip) + getClipFrameDuration(*psgClip))
        data.clear();

    auto* track = psgClip->getClipTrack();
    auto* chip = track != nullptr ? track->pluginList.findFirstPluginOfType<uZX::AYChipPlugin>() : nullptr;
    const double clockHz = chip != nullptr
        ? chip->staticParams.chipClock.getStoredValue() * uZX::MHz
        : uZX::ChipClockEnum::clockValues[uZX::ChipClockEnum::ZX_Spectrum_1_77_MHz];
    return formatPsgFrame(data, clockHz);
}

void PsgClipComponent::showFrameHover() {
    auto* top = getTopLevelComponent();
    const auto mouse = getMouseXYRelative();
    const auto text = describeFrameAt(mouse.x);
    if (top == this || text.empty())
        return;

    frameHover_ = std::make_unique<FrameHover>();
    frameHover_->setText(text);
    top->addAndMakeVisible(*frameHover_);

    // Same placement as MoLookAndFeel::getTooltipBounds: away from the cursor, towards the centre.
    const auto pos = top->getLocalPoint(this, mouse);
    const auto area = top->getLocalBounds();
    const int w = frameHover_->getWidth(), h = frameHover_->getHeight();
    frameHover_->setBounds(Rectangle<int>(pos.x > area.getCentreX() ? pos.x - (w + 12) : pos.x + 24,
                                          pos.y > area.getCentreY() ? pos.y - (h + 6) : pos.y + 6,
                                          w, h).constrainedWithin(area));
}

void PsgClipComponent::hideFrameHover() {
    stopTimer();
    frameHover_.reset();
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

    g.setFont(Font(FontOptions(12.0f).withStyle("Bold")));

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
                const auto hexValue = String::toHexString(value).paddedLeft('0', 2).toUpperCase();
                g.setColour(color);
                g.fillRect(x, y, vis.pixelsPerFrame, laneHeight);
                g.setColour(Colours::black);
                g.drawText(hexValue, Rectangle<float>(x, y, vis.pixelsPerFrame, laneHeight), Justification::centred);
            } else {
                const auto brightness = static_cast<float>(value) / 255.0f;
                g.setColour(color.withAlpha(0.5f + brightness / 2.0f));
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
    constexpr float NoiseSpreadInNoteHeights = 0.75f;
    const int noiseSpread = static_cast<int>(std::floor(noteHeight * NoiseSpreadInNoteHeights));

    auto normToY = [&] (float norm) {
        return (pitchRange.getEnd() - norm) / pitchRange.getLength() * static_cast<float>(vis.rect.getHeight());
    };
    const auto visibleTime = editViewState.zoom.getRange();
    const Range<float> visibleX {vis.timeToX(visibleTime.getStart()), vis.timeToX(visibleTime.getEnd())};
    const Rectangle<float> viewport {visibleX.getStart(), 0.0f, visibleX.getLength(), float(vis.rect.getHeight())};

    vis.forEachVisibleFrame(frames, *psgClip, [&] (int frameIndex, const PsgParamFrame& frame, float x) {
        const auto& frameData = frame.getData();
        const auto envShape = static_cast<uint8_t>(frameData.getRaw(PsgParamType::EnvelopeShape));

        const auto noiseLevels = drawMods ? getNoiseChannelLevels(frameData) : std::array<float, 3> {};
        const auto noiseLevel = *std::max_element(noiseLevels.begin(), noiseLevels.end());
        Rectangle<float> noiseBounds;
        if (noiseLevel > 0.0f) {
            const auto period = static_cast<int>(frameData.getRaw(PsgParamType::NoisePeriod));
            const auto noiseRange = getNoisePeriodYRange(period);
            const float noiseY = noiseRange.getStart() * static_cast<float>(vis.rect.getHeight());
            const float noiseH = jmax(2.0f, noiseRange.getLength() * static_cast<float>(vis.rect.getHeight()));
            noiseBounds = {x, noiseY, vis.pixelsPerFrame, noiseH};
            drawNoiseBand(g, noiseBounds, 0.15f + 0.1f * noiseLevel);
        }

        // Collect this frame's notes: up to one per channel plus the envelope
        std::array<FrameNote, 4> notes;
        int noteCount = 0;
        uint8_t toneChannels = 0, pitchChannels = 0;

        visitFrameNotes(frameData, [&] (const PsgFrameNote& n) {
            pitchChannels |= n.channelMask;
            if (! n.isEnvelopePitch)
                toneChannels |= n.channelMask;
            const float alpha = (n.hasEnvMod || n.isEnvelopePitch)
                ? 1.0f
                : (static_cast<float>(n.volume) / 15.0f * 0.8f + 0.2f);
            const float top = normToY(n.pitch) - noteHeight * 0.5f;
            const float topRound = static_cast<float>(roundToInt(top));
            const float heightRound = static_cast<float>(roundToInt(top + noteHeight)) - topRound;
            notes[noteCount++] = {
                topRound,
                heightRound,
                alpha,
                n.channelMask,
                n.isEnvelopePitch,
                n.hasEnvMod
            };
        });

        const std::span<const FrameNote> pitchNotes(notes.data(), static_cast<size_t>(noteCount));
        if (noiseLevel > 0.0f)
            drawNoiseFringe(g, {x, float(roundToInt(noiseBounds.getCentreY())), vis.pixelsPerFrame, 0.0f},
                           noiseSpread, static_cast<uint8_t>(~pitchChannels & 0x7u), noiseLevels, frameIndex,
                           viewport, pitchNotes);

        std::sort(notes.begin(), notes.begin() + noteCount, [] (const FrameNote& a, const FrameNote& b) {
            return std::tie(a.y, a.isEnvelopePitch, a.channelMask)
                 < std::tie(b.y, b.isEnvelopePitch, b.channelMask);
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
            if (noiseLevel > 0.0f) {
                uint8_t owners = 0;
                for (int i = groupStart; i < gi; ++i)
                    owners |= notes[i].isEnvelopePitch
                        ? static_cast<uint8_t>(notes[i].channelMask & ~toneChannels) : notes[i].channelMask;
                drawNoiseFringe(g, {x, groupY, vis.pixelsPerFrame, groupH}, noiseSpread,
                               owners, noiseLevels, frameIndex, viewport, pitchNotes);
            }

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

                const auto noteColor = note.isEnvelopePitch ? Colors::PSG::Env : getChannelMaskColor(note.channelMask);
                const auto noteRect = Rectangle<float>(x, subY, vis.pixelsPerFrame, jmax(1.0f, subH));

                if (note.isEnvelopePitch) {
                    if (drawMods)
                        drawEnvelopeStripes(
                            g,
                            noteRect.getX(),
                            noteRect.getY(),
                            noteRect.getWidth(),
                            noteRect.getHeight(),
                            envShape,
                            noteColor
                        );
                    g.setColour(getChannelMaskColor(note.channelMask));
                    g.drawRect(noteRect, 1.0f);
                } else {
                    g.setColour(noteColor.withAlpha(note.alpha));
                    g.fillRect(noteRect);

                    if (drawMods && note.hasEnvMod)
                        drawEnvelopeStripes(
                            g,
                            noteRect.getX(),
                            noteRect.getY(),
                            noteRect.getWidth(),
                            noteRect.getHeight(),
                            envShape,
                            noteColor.brighter(0.8f).withAlpha(0.85f)
                        );
                }
            }
        }
        if (envelopePeriodToTonePeriod(frameData.getRaw(PsgParamType::EnvelopePeriod))
            > PsgParamType{PsgParamType::TonePeriodA}.getScale().end) {
            uint8_t owners = 0;
            for (int ch = 0; ch < 3; ++ch)
                if (frameData.getRaw(PsgParamType(PsgParamType::EnvelopeIsOnA + ch)) > 0)
                    owners |= static_cast<uint8_t>(1u << ch);
            if (owners != 0) {
                const auto visible = Range<float>(x, x + vis.pixelsPerFrame).getIntersectionWith(visibleX);
                const float height = drawMods ? jlimit(8.0f, 12.0f, noteHeight * 1.5f)
                                              : jlimit(1.0f, 3.0f, noteHeight);
                const float width = jmin(visible.getLength(), jmin(18.0f, jmax(8.0f, height * 1.6f)));
                if (width > 0.0f)
                    drawLowEnvelopeArrow(g, {visible.getStart() + (visible.getLength() - width) * 0.5f,
                                            float(vis.rect.getHeight()) - height, width, height}, owners, drawMods);
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
    auto row = g.getClipBounds().removeFromTop(swatchSize + 2 * pad).reduced(pad, pad);

    // Channel colors and the envelope-pitch appearance on the left.
    enum class LegendStyle { Channel, Envelope, Noise };
    struct LegendItem { const char* label; Colour color; LegendStyle style = LegendStyle::Channel; };
    const LegendItem items[] = {
        { "A", Colors::PSG::A },
        { "B", Colors::PSG::B },
        { "C", Colors::PSG::C },
        { "E", Colors::PSG::Env, LegendStyle::Envelope },
        { "N", Colors::PSG::Mix, LegendStyle::Noise },
    };

    g.setFont(Font(FontOptions((float) swatchSize - 1.0f).withStyle("Bold")));
    for (const auto& item : items) {
        const int swatchWidth = jmax(
            swatchSize,
            GlyphArrangement::getStringWidthInt(g.getCurrentFont(), item.label) + 4
        );
        auto swatch = row.removeFromLeft(swatchWidth);
        if (item.style == LegendStyle::Noise) {
            drawNoiseBand(g, swatch.toFloat(), 0.9f);
        } else if (item.style == LegendStyle::Envelope) {
            drawEnvelopeStripes(g, (float) swatch.getX(), (float) swatch.getY(),
                                (float) swatch.getWidth(), (float) swatch.getHeight(),
                                0x8, item.color.withAlpha(0.25f));
            g.setColour(item.color);
            g.drawRect(swatch);
        } else {
            g.setColour(item.color);
            g.fillRect(swatch);
        }
        g.setColour(item.style == LegendStyle::Channel ? Colours::black : Colours::white);
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
