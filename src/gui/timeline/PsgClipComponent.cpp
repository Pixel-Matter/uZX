#include <JuceHeader.h>

#include "PsgClipComponent.h"
#include "TuningPickerComponent.h"
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
        case 0x3: return Colors::PSG::AB;
        case 0x4: return Colors::PSG::C;
        case 0x5: return Colors::PSG::AC;
        case 0x6: return Colors::PSG::BC;
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

/** drawNoiseBand at full opacity into a one-pixel-wide strip heightPx tall. */
Image renderNoiseBandStrip(int heightPx) {
    Image strip(Image::ARGB, 1, heightPx, true);
    Graphics g(strip);
    drawNoiseBand(g, {0.0f, 0.0f, 1.0f, float(heightPx)}, 1.0f);
    return strip;
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

}  // namespace

/** x of an edit time in a clip component: the timeline's own x, less the whole-pixel x where
    TrackBodyComponent places the component. Clip contents line up with the ruler to the pixel,
    and the view's edges fall on whole pixels, so nothing pinned to them shifts while scrolling. */
struct ClipXMapping {
    te::TimePosition viewStart;
    te::TimeDuration timePerPixel;
    double originX;

    ClipXMapping(const ZoomViewState& zoom, te::TimePosition clipStart)
        : viewStart(zoom.getStart())
        , timePerPixel(zoom.getTimePerPixel())
        , originX(roundToInt(zoom.timeToX(clipStart)))  // as TrackBodyComponent::resized()
    {
    }

    float operator()(te::TimePosition time) const {
        return static_cast<float>((time - viewStart) / timePerPixel - originX);
    }

    te::TimePosition xToTime(float x) const {
        return viewStart + timePerPixel * (static_cast<double>(x) + originX);
    }
};

/** Where a painter draws: edit time to x in its Graphics' coordinates, and the x extent of the
    clip's content that counts as in view. Painters cover only what the Graphics' clip bounds
    leave of that extent. */
struct PsgClipComponent::PaintView {
    ClipXMapping toX;
    Range<float> visibleX;
};

namespace {

/** The frame-to-pixel mapping shared by the notes and registers painters. */
struct ClipVisibility {
    Rectangle<int> rect;
    te::TimeRange clipRange;
    ClipXMapping toX;
    te::TimeDuration frameDur;
    te::TimeRange range;   // visible part of the clip within the damage, with one frame of slack on the left
    int startIdx;          // first frame that can fall inside range
    float pixelsPerFrame;

    ClipVisibility(const PsgClip& clip,
                   const juce::Array<PsgParamFrame*>& frames,
                   const PsgClipComponent::PaintView& view,
                   Rectangle<int> rectangle,
                   Rectangle<int> damage)
        : rect(rectangle)
        , clipRange(clip.getEditTimeRange())
        , toX(view.toX)
        , frameDur(getClipFrameDuration(clip))
        , range(getRange(view, damage))
        , startIdx(bisectFindPosition(frames, clip, range.getStart()))
        , pixelsPerFrame(static_cast<float>(frameDur / toX.timePerPixel))
    {
    }

    /** The visible extent within the damage, in time; empty, not reversed, when they miss the clip. */
    te::TimeRange getRange(const PsgClipComponent::PaintView& view, Rectangle<int> damage) const {
        const auto start = toX.xToTime(jmax(view.visibleX.getStart(), static_cast<float>(damage.getX()))) - frameDur;
        const auto end = toX.xToTime(jmin(view.visibleX.getEnd(), static_cast<float>(damage.getRight())));
        return clipRange.getIntersectionWith({start, jmax(start, end)});
    }

    float timeToX(te::TimePosition time) const {
        return toX(time);
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

/** The clock of the AY chip on the clip's track, if the track has one. */
std::optional<double> getTrackChipClock(const PsgClip& clip) {
    auto* track = clip.getClipTrack();
    if (auto* chip = track != nullptr ? track->pluginList.findFirstPluginOfType<uZX::AYChipPlugin>() : nullptr)
        return chip->staticParams.chipClock.getStoredValue() * uZX::MHz;
    return std::nullopt;
}

/** Clip properties the tuning and key are read from. */
bool isTuningProperty(const juce::Identifier& property) {
    return property == IDs::clipTuningTable || property == IDs::clipTuningA4 || property == IDs::clipTuningClock
        || property == IDs::clipTuningSystem || property == IDs::clipTuningTonic
        || property == IDs::clipKeyTonic || property == IDs::clipKeyMinor;
}

/** Global app setting: show periods and volumes in hex in the frame hover. */
constexpr auto hexHoverSetting = "psgHoverHexParams";
constexpr auto noteLanesSetting = "showNoteLanes";
constexpr auto pianoKeysSetting = "noteLanesAsPianoKeys";

} // namespace

bool areNoteLanesShown(te::Engine& engine) {
    return engine.getPropertyStorage().getPropertiesFile().getBoolValue(noteLanesSetting, true);
}

void setNoteLanesShown(te::Engine& engine, bool shouldShow) {
    // The settings file broadcasts the change; every PsgClipComponent repaints on it.
    engine.getPropertyStorage().getPropertiesFile().setValue(noteLanesSetting, shouldShow);
}

bool arePianoKeysShown(te::Engine& engine) {
    return engine.getPropertyStorage().getPropertiesFile().getBoolValue(pianoKeysSetting, false);
}

void setPianoKeysShown(te::Engine& engine, bool shouldShow) {
    engine.getPropertyStorage().getPropertiesFile().setValue(pianoKeysSetting, shouldShow);
}

String getClockMismatchText(double tuningClock, double trackClock) {
    const int cents = roundToInt(1200.0 * std::log2(trackClock / tuningClock));
    if (cents == 0)
        return {};
    return String::fromUTF8("\xE2\x9A\xA0 ") + getClockName(tuningClock) + " @ " + getClockName(trackClock)  // ⚠
         + String::formatted(" %+d", cents) + String::fromUTF8("\xC2\xA2");                                  // ¢
}

std::vector<std::pair<String, Colour>> formatPsgFrame(const PsgParamFrameData& data, double chipClockHz, bool hex) {
    using T = PsgParamType;
    std::vector<std::pair<String, Colour>> runs;
    Colour rowColour;
    const auto add = [&] (const String& text, bool changed = false) {
        runs.emplace_back(text, changed ? rowColour : rowColour.withMultipliedAlpha(0.5f));
    };
    const auto hz = [&] (int tonePeriod) {
        return String(chipClockHz / 16.0 / jmax(1, tonePeriod), 1) + "Hz";  // AY tone clock divider
    };
    const auto number = [hex] (int value, int hexDigits) {
        return hex ? String::toHexString(value).paddedLeft('0', hexDigits).toUpperCase() : String(value);
    };

    // Rows A, B, C, Env, Noise share the period and Hz columns.
    const int envPeriod = data.getRaw(T::EnvelopePeriod);
    std::array<String, 5> periods, freqs, volumes;
    for (size_t ch = 0; ch < 3; ++ch) {
        const int period = data.getRaw(T(T::TonePeriodA + int(ch)));
        periods[ch] = number(period, 3);
        freqs[ch] = hz(period);
        volumes[ch] = number(data.getRaw(T(T::VolumeA + int(ch))), 1);
    }
    periods[3] = number(envPeriod, 4);
    freqs[3] = hz(envelopePeriodToTonePeriod(envPeriod));
    periods[4] = number(data.getRaw(T::NoisePeriod), 2);
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
class PsgClipComponent::ContentTiles {
public:
    /** Logical px: 512 device px on a Retina display. Each frame blits the visible tiles, about
        ten per 2500 px view, and paints one slice of the tile about to scroll in. */
    static constexpr int tileWidth = 256, sliceWidth = 64;

    /** What the cached pixels depend on besides the time-to-x mapping. */
    struct Key {
        te::TimeDuration timePerPixel;
        te::ClipPosition position;
        int width, height, scalePercent, revision;
        uint32 background;  // the lane lines are opaque, blended onto it

        bool operator==(const Key& o) const {
            return timePerPixel == o.timePerPixel && position.time == o.position.time
                && position.offset == o.position.offset && width == o.width && height == o.height
                && scalePercent == o.scalePercent && revision == o.revision && background == o.background;
        }
    };

    ContentTiles(const Key& k, const ClipXMapping& x) : key(k), toX(x) {}

    /** Whole pixels from the tiles' content x to the component's, or none when the view has
        moved by a fraction of a pixel since the tiles were painted. */
    std::optional<int> getShift(const ClipXMapping& now) const {
        const double shift = (toX.viewStart - now.viewStart) / toX.timePerPixel + (toX.originX - now.originX);
        const int whole = roundToInt(shift);
        if (std::abs(shift - whole) > 0.01)
            return std::nullopt;
        return whole;
    }

    using Painter = std::function<void(Graphics&, const PaintView&)>;

    /** Blits the tiles in damage, painting what they lack, and paints a slice ahead of shown.
        Rectangles are in the component's coordinates. */
    void paint(Graphics& g, int shift, float scale, Rectangle<int> damage, Rectangle<int> shown, const Painter& painter) {
        const auto firstOf = [&](int x) { return floorDiv(x - shift); };
        const int lastTile = (key.width - 1) / tileWidth;
        const int keepFirst = firstOf(shown.getX()) - 1, keepLast = firstOf(shown.getRight() - 1) + 1;
        for (auto it = tiles.begin(); it != tiles.end();)
            it = it->first < keepFirst || it->first > keepLast ? tiles.erase(it) : std::next(it);

        g.setOpacity(1.0f);
        for (int i = jmax(0, firstOf(damage.getX())); i <= jmin(lastTile, firstOf(damage.getRight() - 1)); ++i) {
            auto& tile = getTile(i, scale);
            paintTile(tile, i, tileWidth, scale, painter);
            g.drawImageTransformed(tile.image, AffineTransform::scale(1.0f / scale)
                                                   .translated(static_cast<float>(i * tileWidth + shift), 0.0f));
        }

        // ponytail: a fixed slice keeps up with views scrolling up to sliceWidth / tileWidth of a tile
        // a frame; faster, the rest is painted as the tile shows. Make it follow the scroll speed if that shows.
        if (const int ahead = keepLast; ahead <= lastTile)
            paintTile(getTile(ahead, scale), ahead, sliceWidth, scale, painter);
    }

    /** Blits the note lanes, which hold nothing that moves with time: the gutter as it is, and
        right of it, over grid, one device pixel column of the grid stretched. Each is painted by
        painter, in this component's coordinates, when its size changes. */
    void paintLanes(Graphics& g, Rectangle<int> gutter, Rectangle<int> grid, float scale,
                    const std::function<void(Graphics&)>& painter) {
        const auto update = [&](Image& image, Rectangle<int> area, int width) {
            const int height = roundToInt(area.getHeight() * scale);
            if (image.getWidth() == width && image.getHeight() == height)
                return;
            image = Image(Image::ARGB, width, height, true);
            Graphics ig(image);
            ig.addTransform(AffineTransform::scale(scale));
            ig.setOrigin(-area.getPosition());
            painter(ig);
        };
        Graphics::ScopedSaveState state(g);
        g.setOpacity(1.0f);
        if (! gutter.isEmpty()) {
            update(gutterImage, gutter, roundToInt(gutter.getWidth() * scale));
            g.drawImageTransformed(gutterImage, AffineTransform::scale(1.0f / scale).translated(gutter.getPosition().toFloat()));
        }
        if (! grid.isEmpty()) {
            update(gridColumn, grid, 1);
            g.setImageResamplingQuality(Graphics::lowResamplingQuality);  // copies of the column, not blends
            g.drawImage(gridColumn, grid.toFloat());
        }
    }

    const Key key;
    const ClipXMapping toX;  // the content's x, as of when the tiles were first painted

private:
    Image gutterImage, gridColumn;
    struct Tile {
        Image image;
        int painted = 0;  // px painted from the tile's left
    };
    std::map<int, Tile> tiles;

    static int floorDiv(int x) { return x >= 0 ? x / tileWidth : -((tileWidth - 1 - x) / tileWidth); }

    Tile& getTile(int index, float scale) {
        auto& tile = tiles[index];
        if (! tile.image.isValid())
            tile.image = Image(Image::ARGB, roundToInt(tileWidth * scale), roundToInt(key.height * scale), true);
        return tile;
    }

    /** Paints up to width more px of the tile, left to right. */
    void paintTile(Tile& tile, int index, int width, float scale, const Painter& painter) const {
        const int end = jmin(tileWidth, tile.painted + width);
        if (end <= tile.painted)
            return;
        {
            Graphics g(tile.image);
            g.addTransform(AffineTransform::scale(scale));
            g.reduceClipRegion(tile.painted, 0, end - tile.painted, key.height);
            g.setOrigin({-index * tileWidth, 0});
            painter(g, {toX, {0.0f, static_cast<float>(key.width)}});
        }
        tile.painted = end;
    }
};

//==============================================================================
PsgClipComponent::PsgClipComponent(EditViewState& evs, TimelineGrid& g, te::Clip::Ptr c)
    : MidiClipComponent(evs, g, c)
    // , vblankAttachment_(this, [this](double) { onVBlank(); })
{
    // setBufferedToImage(true);
    // Tempo and the clip's tuning, see valueTreePropertyChanged().
    editViewState.edit.state.addListener(this);
    // App settings such as Note Lanes. Track bodies are buffered to images, so a window
    // repaint would not reach this component: it has to repaint itself.
    clip->edit.engine.getPropertyStorage().getPropertiesFile().addChangeListener(this);
}

PsgClipComponent::~PsgClipComponent() {
    clip->edit.engine.getPropertyStorage().getPropertiesFile().removeChangeListener(this);
    editViewState.edit.state.removeListener(this);
}

void PsgClipComponent::changeListenerCallback(ChangeBroadcaster*) {
    ++contentRevision_;
    repaint();
}

PsgClip* PsgClipComponent::getPsgClip() {
    return dynamic_cast<PsgClip*>(clip.get());
}

void PsgClipComponent::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) {
    // Frames are placed at the effective fps: frames-per-beat at the current beat length.
    if (property == te::IDs::bpm || property == te::IDs::startBeat) {
        ++contentRevision_;
        repaint();
    }
    // The listener sits on the whole edit; another clip's tuning is not ours to repaint.
    else if ((isTuningProperty(property) || property == IDs::paintMode) && tree == clip->state) {
        ++contentRevision_;
        repaint();
    }
    // Warnings over the notes compare with the project's fps and the track's clock, as the header's do
    else if (! editViewState.showClipHeaders && (property == IDs::projectFps || property == uZX::IDs::clock))
        repaint();
}

void PsgClipComponent::paint(Graphics& g) {
    ClipComponent::paint(g);

    GUIPaintMeasurer::ScopedTimer timer(paintMeasurer_);

    const auto view = getViewPaintView();
    if (shouldCacheContent()) {
        paintCachedContent(g, view);
    } else {
        tiles_.reset();
        paintContent(g, view);
    }
    if (! editViewState.showClipHeaders)
        paintWarnings(g);

   #if JUCE_DEBUG
    paintMeasurer_.drawOverlay(g, getShownArea(*this));
   #endif
}

PsgClipComponent::PaintView PsgClipComponent::getViewPaintView() const {
    const ClipXMapping toX {editViewState.zoom, clip->getEditTimeRange().getStart()};
    const auto viewTime = editViewState.zoom.getRange();
    // The view and the clip overlap only partly: a clip starting later than the view's
    // start would put the view's left edge at a negative x, outside the component.
    const Range<float> viewX {toX(viewTime.getStart()), toX(viewTime.getEnd())};
    return {toX, viewX.getIntersectionWith({0.0f, static_cast<float>(getWidth())})};
}

void PsgClipComponent::paintContent(Graphics& g, const PaintView& view) {
    if ((int) clip->state.getProperty(IDs::paintMode, 0) == 0) {
        paintNoteLanes(g, view);
        paintTicksOverLanes(g, view);
        paintNotes(g, view);
    } else {
        paintRegisters(g, view);
    }
}

/** The right-click menu of a PSG clip's body and header. */
static void showPsgClipMenu(PsgClip& psgClip) {
    const int mode = psgClip.state.getProperty(IDs::paintMode, 0);

    // The body repaints on the paint mode change.
    PopupMenu m;
    m.addItem("Notes", true, mode == 0, [clip = te::Clip::Ptr(&psgClip)] {
        clip->state.setProperty(IDs::paintMode, 0, nullptr);
    });
    m.addItem("Registers", true, mode == 1, [clip = te::Clip::Ptr(&psgClip)] {
        clip->state.setProperty(IDs::paintMode, 1, nullptr);
    });
    auto& engine = psgClip.edit.engine;
    m.addItem("Note Lanes", mode == 0, areNoteLanesShown(engine), [&engine] {
        setNoteLanesShown(engine, ! areNoteLanesShown(engine));
    });
    m.addItem("Piano Keys", mode == 0 && areNoteLanesShown(engine), arePianoKeysShown(engine), [&engine] {
        setPianoKeysShown(engine, ! arePianoKeysShown(engine));
    });
    m.addSeparator();
    auto& settings = engine.getPropertyStorage().getPropertiesFile();
    m.addItem("Hex params in hover", true, settings.getBoolValue(hexHoverSetting), [&settings] {
        settings.setValue(hexHoverSetting, ! settings.getBoolValue(hexHoverSetting));
    });
    m.addSeparator();
    m.addItem("Tuning and key...", psgClip.getTuningDetection().hasNotes, false,
              [safeClip = te::SafeSelectable<PsgClip>(&psgClip)] {
                  if (auto* c = safeClip.get())
                      TuningPickerComponent::show(*c);
              });
    m.showMenuAsync({});
}

void PsgClipComponent::mouseDown(const MouseEvent& e) {
    hideFrameHover();
    if (! e.mods.isPopupMenu())
        ClipComponent::mouseDown(e);
    else if (auto* psgClip = getPsgClip())
        showPsgClipMenu(*psgClip);
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

    const double clockHz = getTrackChipClock(*psgClip).value_or(tuningGridClock);
    const bool hex = clip->edit.engine.getPropertyStorage().getPropertiesFile().getBoolValue(hexHoverSetting);
    return formatPsgFrame(data, clockHz, hex);
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

void PsgClipComponent::paintRegisters(Graphics& g, const PaintView& view) {
    auto* psgClip = getPsgClip();
    if (psgClip == nullptr) return;

    const auto& frames = psgClip->getPsg().getFrames();
    if (frames.isEmpty())
        return;

    const ClipVisibility vis {*psgClip, frames, view, getLocalBounds(), g.getClipBounds()};

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

/** Vertical mapping of the Notes view: the clip's pitch range over the component
    height, the height of one semitone, and the visible slice of the clip in x. */
struct NotesLayout {
    Range<float> pitchRange;
    float noteHeight;
    Range<float> visibleX;  // as PaintView's
    float height;

    float normToY(float norm) const {
        return (pitchRange.getEnd() - norm) / pitchRange.getLength() * height;
    }
};

static NotesLayout makeNotesLayout(const PsgClip& clip, Rectangle<int> bounds, Range<float> visibleX) {
    const auto pitchRange = clip.getPitchRange();
    const auto height = static_cast<float>(bounds.getHeight());
    return {
        pitchRange,
        height / (pitchRange.getLength() * PsgParamType{PsgParamType::TonePeriodA}.getScale().octaves() * 12.0f),
        visibleX,
        height
    };
}

std::vector<NoteLane> makeNoteLanes(const PeriodGrid& grid) {
    // Grid notes by rising pitch, in the normalized space the painter uses, so a lane is
    // centred exactly where a note on that grid period is drawn.
    const auto toneScale = PsgParamType{PsgParamType::TonePeriodA}.getScale();
    std::vector<std::pair<float, int>> notes;
    notes.reserve(grid.size() + 48);
    for (auto it = grid.rbegin(); it != grid.rend(); ++it)
        notes.emplace_back(toneScale.valueToNormalized(it->first), it->second);
    if (notes.size() < 2)
        return {};

    // Tables stop at C1 and B8, but tone periods reach down to the octave below C1 and up
    // to period 8, up to a whole octave above B8. Lanes continue past both ends by octaves
    // of the outermost table octave, with exact (unrounded) periods so they stay even.
    std::map<int, double> periodOf;
    for (const auto& [period, note] : grid)
        periodOf.try_emplace(note, period);
    const int lowest = periodOf.begin()->first, highest = periodOf.rbegin()->first;
    std::vector<std::pair<float, int>> below, above;
    for (int note = lowest - 1; note >= 0; --note) {
        auto octaveUp = periodOf.find(note + 12);
        if (octaveUp == periodOf.end() || octaveUp->second * 2.0 > toneScale.end * 2.0)
            break;
        periodOf[note] = octaveUp->second * 2.0;
        below.emplace_back(toneScale.valueToNormalized(static_cast<float>(periodOf[note])), note);
    }
    for (int note = highest + 1;; ++note) {
        auto octaveDown = periodOf.find(note - 12);
        if (octaveDown == periodOf.end() || octaveDown->second / 2.0 < 4.0)
            break;
        periodOf[note] = octaveDown->second / 2.0;
        above.emplace_back(toneScale.valueToNormalized(static_cast<float>(periodOf[note])), note);
    }
    notes.insert(notes.begin(), below.rbegin(), below.rend());
    notes.insert(notes.end(), above.begin(), above.end());

    std::vector<NoteLane> lanes;
    lanes.reserve(notes.size());
    for (size_t i = 0; i < notes.size(); ++i) {
        const float centre = notes[i].first;
        // The outermost lanes mirror their inner neighbour's spacing.
        const float below = i > 0 ? notes[i - 1].first : centre - (notes[i + 1].first - centre);
        const float above = i + 1 < notes.size() ? notes[i + 1].first : centre + (centre - notes[i - 1].first);
        // Composed rather than getMidiNoteName(): lanes above B8 go past MIDI note 127.
        const int note = notes[i].second;
        lanes.push_back({ (below + centre) * 0.5f, (centre + above) * 0.5f, note,
                          MidiMessage::getMidiNoteName(note % 12, true, false, 4) + String(note / 12 - 1) });
    }
    return lanes;
}

// A line under every lane needs a few pixels, names about a line of small text. When a single
// lane is too short, one per octave can still be shown, down to a few pixels an octave: the line
// under each C, and the tonic's name. Piano keys need lanes tall enough to tell apart.
constexpr float minLaneHeight = 4.0f, minOctaveHeight = 3.0f, minLabelHeight = 8.0f, minPianoKeyHeight = 4.0f;

// A black key holds a sharp's name, C#4; right of it, a white key holds its own.
constexpr float whiteKeyWidth = 32.0f, blackKeyWidth = 18.0f, keyNameGap = 2.0f;
constexpr float laneLabelGap = 3.0f, laneLabelWidth = 40.0f;

LaneDetail getLaneDetail(float noteHeight) {
    auto level = [noteHeight](float minLane, float minOctave) {
        return noteHeight >= minLane ? LaneDetail::All
             : noteHeight * 12.0f >= minOctave ? LaneDetail::Tonic
             : LaneDetail::None;
    };
    return { level(minLaneHeight, minOctaveHeight), level(minLabelHeight, minLabelHeight) };
}

/** The note lanes as the Notes view draws them now. */
struct LaneView {
    NotesLayout layout;
    LaneDetail detail;
    bool pianoKeys;  // keys too short to tell apart leave just the grid

    /** Where the names start: right of the black keys when the keys are shown, but a black
        key's own name starts at its left end. */
    float getLabelX() const {
        return layout.visibleX.getStart() + (pianoKeys ? blackKeyWidth + keyNameGap : laneLabelGap);
    }
    /** Where the keys and names end: right of it is clear of them. */
    float getGutterRight() const {
        if (pianoKeys)
            return layout.visibleX.getStart() + whiteKeyWidth;
        return getLabelX() + (detail.labels != LaneDetail::None ? laneLabelWidth : 0.0f);
    }
    /** The whole pixels of the view's part of the clip, and where the grid starts in them, right of
        the keys and names. */
    Range<int> getViewX() const {
        return {static_cast<int>(std::floor(layout.visibleX.getStart())),
                static_cast<int>(std::ceil(layout.visibleX.getEnd()))};
    }
    int getGridX() const {
        const auto viewX = getViewX();
        if (detail.labels == LaneDetail::None && ! pianoKeys)
            return viewX.getStart();
        return jlimit(viewX.getStart(), viewX.getEnd(), static_cast<int>(std::ceil(getGutterRight())));
    }
};

/** None when the lanes are off, the clip is empty, the tuning has no grid, or the lanes are
    too short to draw. */
static std::optional<LaneView> getLaneView(const PsgClip& clip, Rectangle<int> bounds, Range<float> visibleX) {
    if (clip.getPsg().isEmpty() || ! areNoteLanesShown(clip.edit.engine) || clip.getTuning().grid.size() < 2)
        return std::nullopt;
    const auto layout = makeNotesLayout(clip, bounds, visibleX);
    auto detail = getLaneDetail(layout.noteHeight);
    if (detail.lines == LaneDetail::None)
        return std::nullopt;
    const bool pianoKeys = arePianoKeysShown(clip.edit.engine) && layout.noteHeight >= minPianoKeyHeight;
    // A tonic's name alone would spill over the keys around it, which show where C is anyway.
    if (pianoKeys && detail.labels == LaneDetail::Tonic)
        detail.labels = LaneDetail::None;
    return LaneView {layout, detail, pianoKeys};
}

/** Piano Keys mode: a short keyboard over the grid at the view's left edge, just enough to
    show the groups of two and three black keys that tell where C is. Each lane is one key;
    white keys meet halfway along the black key between them, as on a piano. */
static void paintPianoKeys(Graphics& g, const std::vector<NoteLane>& lanes, const NotesLayout& layout) {
    const float x = layout.visibleX.getStart();
    const auto damage = g.getClipBounds().toFloat();
    if (damage.getX() >= x + whiteKeyWidth || damage.getRight() <= x)
        return;

    g.setColour(Colours::white.withAlpha(0.85f));
    g.fillRect(x, 0.0f, whiteKeyWidth, layout.height);
    for (const auto& lane : lanes) {
        // Whole pixels, so neighbouring keys neither overlap nor leave seams.
        const float top = std::round(layout.normToY(lane.hi)), bottom = std::round(layout.normToY(lane.lo));
        if (bottom < damage.getY() || top > damage.getBottom())
            continue;

        if (MidiMessage::isMidiNoteBlack(lane.note)) {
            g.setColour(Colours::black.withAlpha(0.85f));
            g.fillRect(x, top, blackKeyWidth, bottom - top);
            g.setColour(Colours::black.withAlpha(0.5f));
            g.drawHorizontalLine(roundToInt((top + bottom) * 0.5f), x + blackKeyWidth, x + whiteKeyWidth);
        } else if (lane.note % 12 == 0 || lane.note % 12 == 5) {  // B|C and E|F have no black key between
            g.setColour(Colours::black.withAlpha(0.5f));
            g.drawHorizontalLine(roundToInt(bottom), x, x + whiteKeyWidth);
        }
    }
}

/** Piano-roll lanes for the clip's tuning grid, under the notes, which are never snapped
    to them: drift, bends and portamento stay where the chip plays them. The lane list is
    rebuilt only when the clip's tuning changes; each paint draws the lanes that intersect
    the repaint area. */
void PsgClipComponent::paintNoteLanes(Graphics& g) {
    paintNoteLanes(g, getViewPaintView());
}

void PsgClipComponent::paintNoteLanes(Graphics& g, const PaintView& paintView) {
    auto* psgClip = getPsgClip();
    const auto view = psgClip != nullptr ? getLaneView(*psgClip, getLocalBounds(), paintView.visibleX) : std::nullopt;
    if (! view)
        return;

    const auto& tuning = psgClip->getTuning();
    const auto& layout = view->layout;
    const auto detail = view->detail;
    const bool pianoKeys = view->pianoKeys;

    if (lanesRevision_ != tuning.revision) {
        lanes_ = makeNoteLanes(tuning.grid);
        lanesRevision_ = tuning.revision;
    }

    // Without a key the octave anchor falls back to C and no lane is shaded; the keys name C major.
    const int tonic = tuning.hasKey ? static_cast<int>(tuning.key.tonic) : 0;
    const Scale scale(tuning.hasKey ? tuning.key.getScaleType() : Scale::ScaleType(Scale::ScaleType::IonianOrMajor));
    const float x = layout.visibleX.getStart(), right = layout.visibleX.getEnd();
    const float gridX = pianoKeys ? x + whiteKeyWidth : x;  // the keys take the grid's left end
    const float labelX = view->getLabelX(), labelWidth = view->getGutterRight() - labelX;
    const auto damage = g.getClipBounds().toFloat();
    constexpr float tonicLabelHeight = 10.0f;
    // As the timeline grid's finest and medium ticks look on the clip
    const auto background = getBackground();
    const auto laneLine = background.overlaidWith(Colors::Timeline::trackGridTickColors[0]);
    const auto octaveLine = background.overlaidWith(Colors::Timeline::trackGridTickColors[1]);
    const bool labelsInDamage = detail.labels != LaneDetail::None
        && damage.getX() < labelX + labelWidth && damage.getRight() > x;
    if (labelsInDamage)
        g.setFont(Font(FontOptions(detail.labels == LaneDetail::All ? jmin(11.0f, layout.noteHeight - 2.0f) : 9.0f)));
    if (pianoKeys)
        paintPianoKeys(g, lanes_, layout);

    for (const auto& lane : lanes_) {
        const float top = layout.normToY(lane.hi), bottom = layout.normToY(lane.lo);
        // A tonic label hangs above the lane's bottom boundary and may overflow a short lane.
        if (bottom < damage.getY() || jmin(top, bottom - tonicLabelHeight) > damage.getBottom())
            continue;

        const int degree = ((lane.note - tonic) % 12 + 12) % 12;
        const bool isTonic = degree == 0;
        if (tuning.hasKey && isTonic && detail.lines == LaneDetail::All) {
            // In the lines' colour and on their rows, from the line above to its own: one even stripe,
            // with no soft edge or gap against them. Its own line, an octave's on C, goes over it.
            const int bandTop = roundToInt(top), bandBottom = roundToInt(bottom);
            g.setColour(laneLine);
            g.fillRect(gridX, static_cast<float>(bandTop), right - gridX, static_cast<float>(bandBottom - bandTop + 1));
        }
        // Octaves start on C whatever the key; the lines are opaque, so the timeline grid under them
        // does not add up to a brighter dot where they cross
        const bool octave = lane.note % 12 == 0;
        if (detail.lines == LaneDetail::All || octave) {
            g.setColour(octave ? octaveLine : laneLine);
            g.drawHorizontalLine(roundToInt(bottom), gridX, right);
        }
        if (! labelsInDamage)
            continue;
        if (detail.labels == LaneDetail::All) {
            // Over the keys only the key's notes are named, each on its own key: light at a black
            // key's left end, dark right of the black keys on a white one.
            if (pianoKeys && ! scale.isIntervalInScale(degree))
                continue;
            const bool onBlackKey = pianoKeys && MidiMessage::isMidiNoteBlack(lane.note);
            const float nameX = onBlackKey ? x : labelX;
            const auto ink = pianoKeys && ! onBlackKey ? Colours::black : Colours::white;
            g.setColour(ink.withAlpha(isTonic ? 0.8f : 0.45f));
            g.drawText(lane.label, Rectangle<float>(nameX, top, labelX + labelWidth - nameX, bottom - top),
                       Justification::centredLeft, false);
        } else if (isTonic) {
            g.setColour(Colours::white.withAlpha(0.45f));
            g.drawText(lane.label, Rectangle<float>(labelX, bottom - tonicLabelHeight, labelWidth, tonicLabelHeight),
                       Justification::bottomLeft, false);
        }
    }
}

/** The timeline grid's beat and bar ticks over the lanes' grid, opaque, so where one crosses a lane
    line the brighter of the two shows, not their blend. The finest ticks stay under the lanes, the
    colour of their lines; none cross the keys and names. */
void PsgClipComponent::paintTicksOverLanes(Graphics& g, const PaintView& view) {
    auto* psgClip = getPsgClip();
    const auto lanes = psgClip != nullptr ? getLaneView(*psgClip, getLocalBounds(), view.visibleX) : std::nullopt;
    if (! lanes)
        return;
    const Range<int> gridX {lanes->getGridX(), lanes->getViewX().getEnd()};
    const auto background = getBackground();
    for (const auto& tick : grid.getTicks()) {
        const int x = tick.x - getX();  // ticks are in the track body's x
        if (tick.level == 0 || tick.level >= Colors::Timeline::trackGridTickColors.size() || ! gridX.contains(x))
            continue;
        g.setColour(background.overlaidWith(Colors::Timeline::trackGridTickColors[tick.level]));
        g.drawVerticalLine(x, 0.0f, static_cast<float>(getHeight()));
    }
}

void PsgClipComponent::paintWarnings(Graphics& g) {
    auto* psgClip = getPsgClip();
    if (psgClip == nullptr)
        return;
    constexpr int pad = ClipHeader::pad, chipHeight = 12;
    auto row = getShownArea(*this).reduced(pad).withHeight(chipHeight);
    // clear of the note lanes' keys and names
    if ((int) clip->state.getProperty(IDs::paintMode, 0) == 0)
        if (const auto view = getLaneView(*psgClip, getLocalBounds(), getViewPaintView().visibleX))
            row.setLeft(jmax(row.getX(), roundToInt(view->getGutterRight())));
    for (const auto& warning : getPsgClipWarnings(*psgClip)) {
        const int width = ClipHeader::getWarningWidth(warning);
        if (width > row.getWidth())
            break;
        ClipHeader::paintWarning(g, row.removeFromLeft(width), warning);
        row.removeFromLeft(pad);
    }
}

bool PsgClipComponent::shouldCacheContent() const {
    return editViewState.zoom.isFollowingPlayhead();
}

void PsgClipComponent::paintCachedContent(Graphics& g, const PaintView& view) {
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    const ContentTiles::Key key {view.toX.timePerPixel, clip->getPosition(), getWidth(), getHeight(),
                                 roundToInt(scale * 100.0f), contentRevision_, getBackground().getARGB()};
    auto shift = tiles_ != nullptr && tiles_->key == key ? tiles_->getShift(view.toX) : std::nullopt;
    if (! shift) {
        tiles_ = std::make_unique<ContentTiles>(key, view.toX);
        shift = 0;
    }

    // The lanes, with the names and keys at the view's left edge, from their own images under the
    // notes' tiles: the order paintContent() draws them in.
    const bool notesView = (int) clip->state.getProperty(IDs::paintMode, 0) == 0;
    if (auto* psgClip = getPsgClip(); psgClip != nullptr && notesView)
        if (const auto lanes = getLaneView(*psgClip, getLocalBounds(), view.visibleX)) {
            const auto viewX = lanes->getViewX();
            const int gridX = lanes->getGridX();
            tiles_->paintLanes(g, {viewX.getStart(), 0, gridX - viewX.getStart(), getHeight()},
                               {gridX, 0, viewX.getEnd() - gridX, getHeight()}, scale,
                               [&](Graphics& lg) { paintNoteLanes(lg, view); });
            paintTicksOverLanes(g, view);
        }

    tiles_->paint(g, *shift, scale, g.getClipBounds(), getShownArea(*this),
                  [this, notesView](Graphics& tg, const PaintView& tileView) {
                      if (notesView)
                          paintNotes(tg, tileView);
                      else
                          paintRegisters(tg, tileView);
                  });
}

void PsgClipComponent::paintNotes(Graphics& g) {
    paintNotes(g, getViewPaintView());
}

void PsgClipComponent::paintNotes(Graphics& g, const PaintView& view) {
    auto* psgClip = getPsgClip();
    if (psgClip == nullptr)
        return;

    const auto& frames = psgClip->getPsg().getFrames();
    if (frames.isEmpty())
        return;

    const ClipVisibility vis {*psgClip, frames, view, getLocalBounds(), g.getClipBounds()};
    const auto layout = makeNotesLayout(*psgClip, vis.rect, view.visibleX);

    const auto pitchRange = layout.pitchRange;
    const float noteHeight = layout.noteHeight;
    const bool drawMods = vis.pixelsPerFrame >= 6.0f && noteHeight >= 2.0f;
    constexpr float NoiseSpreadInNoteHeights = 0.75f;
    const int noiseSpread = static_cast<int>(std::floor(noteHeight * NoiseSpreadInNoteHeights));

    auto normToY = [&] (float norm) { return layout.normToY(norm); };
    const Range<float> visibleX = layout.visibleX;
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
            const int stripHeight = roundToInt(noiseH * g.getInternalContext().getPhysicalPixelScaleFactor());
            if (noiseBand_.getHeight() != stripHeight)
                noiseBand_ = renderNoiseBandStrip(stripHeight);  // noiseH >= 2: never empty
            g.setOpacity(0.15f + 0.1f * noiseLevel);
            g.drawImage(noiseBand_, noiseBounds);
            g.setOpacity(1.0f);
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

//==============================================================================
PsgClipHeader::PsgClipHeader(EditViewState& evs, te::Clip::Ptr c)
    : ClipHeader(evs, c)
{
    editViewState.edit.state.addListener(this);
}

PsgClipHeader::~PsgClipHeader() {
    editViewState.edit.state.removeListener(this);
}

void PsgClipHeader::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) {
    // The effective-fps warning depends on frames-per-beat, current beat length
    // and the project fps, so refresh when tempo or fps changes.
    if (property == IDs::projectFps
        || property == te::IDs::bpm
        || property == te::IDs::startBeat)
        repaint();
    // The clock warning compares the tuning with the track's AY clock. Changes are
    // rare, so any chip's clock repaints every PSG clip header.
    else if (property == uZX::IDs::clock)
        repaint();
    else if (isTuningProperty(property) && tree == clip->state)
        repaint();
}

void PsgClipHeader::mouseDown(const MouseEvent& e) {
    if (! e.mods.isPopupMenu())
        ClipHeader::mouseDown(e);
    else if (auto* psgClip = dynamic_cast<PsgClip*>(clip.get()))
        showPsgClipMenu(*psgClip);
}

StringArray getPsgClipWarnings(PsgClip& psgClip) {
    StringArray warnings;
    const auto editFps = Helpers::getProjectFps(psgClip.edit);
    if (psgClip.getPsg().hasFpsMismatch(psgClip, editFps))
        warnings.add(String::fromUTF8("\xE2\x9A\xA0 ")  // ⚠
                     + String(roundToInt(psgClip.getPsg().getEffectiveFps(psgClip))) + " @ "
                     + String(roundToInt(editFps)) + " fps");

    // A fitted tuning names its notes for its clock; a track at another clock plays
    // them off by the clock ratio. Tracker tables have no clock to disagree with.
    if (const auto& tuning = psgClip.getTuning(); tuning.fit)
        if (const auto trackClock = getTrackChipClock(psgClip))
            if (const auto warning = getClockMismatchText(tuning.fit->chipClock, *trackClock); warning.isNotEmpty())
                warnings.add(warning);
    return warnings;
}

String getPsgClipInfo(const PsgClip& psgClip) {
    // Detected (or chosen) tuning table and key; '?' marks a detection tie nobody settled.
    // A clip without audible pitches has neither, unless the user chose one.
    const auto& tuning = psgClip.getTuning();
    auto text = tuning.tableLabel;
    if (tuning.hasKey)
        text << (text.isEmpty() ? "" : "  ") << tuning.key.getName();
    if (text.isNotEmpty() && tuning.uncertain)
        text << " ?";
    return text;
}

StringArray PsgClipHeader::getWarnings() {
    auto* psgClip = dynamic_cast<PsgClip*>(clip.get());
    return psgClip != nullptr ? getPsgClipWarnings(*psgClip) : StringArray();
}

String PsgClipHeader::getInfo() {
    auto* psgClip = dynamic_cast<PsgClip*>(clip.get());
    return psgClip != nullptr ? getPsgClipInfo(*psgClip) : String();
}

namespace {

enum class LegendStyle { Channel, Envelope, Noise };
struct LegendItem { const char* label; Colour color; LegendStyle style = LegendStyle::Channel; };

constexpr int legendSpacing = 1;

Font getLegendFont() {
    return Font(FontOptions(11.0f).withStyle("Bold"));
}

/** Channel colors and the envelope-pitch appearance, with each swatch's width. */
template <typename Fn>
void forEachLegendSwatch(Fn&& fn) {
    const LegendItem items[] = {
        { "A", Colors::PSG::A },
        { "B", Colors::PSG::B },
        { "C", Colors::PSG::C },
        { "E", Colors::PSG::Env, LegendStyle::Envelope },
        { "N", Colors::PSG::Mix, LegendStyle::Noise },
    };
    const auto font = getLegendFont();
    for (const auto& item : items)
        fn(item, jmax(12, GlyphArrangement::getStringWidthInt(font, item.label) + 4));
}

}  // namespace

int getPsgLegendWidth() {
    int width = -legendSpacing;
    forEachLegendSwatch([&](const LegendItem&, int swatchWidth) { width += swatchWidth + legendSpacing; });
    return width;
}

void paintPsgLegend(Graphics& g, Rectangle<int> area) {
    g.setFont(getLegendFont());
    forEachLegendSwatch([&](const LegendItem& item, int swatchWidth) {
        auto swatch = area.removeFromLeft(swatchWidth);
        area.removeFromLeft(legendSpacing);
        if (item.style == LegendStyle::Noise) {
            drawNoiseBand(g, swatch.toFloat(), 0.9f);
        } else if (item.style == LegendStyle::Envelope) {
            drawEnvelopeStripes(g, (float) swatch.getX(), (float) swatch.getY(),
                                (float) swatch.getWidth(), (float) swatch.getHeight(),
                                0x8, item.color);
            g.setColour(item.color);
            g.drawRect(swatch);
        } else {
            g.setColour(item.color);
            g.fillRect(swatch);
        }
        g.setColour(item.style == LegendStyle::Channel ? Colours::black : Colours::white);
        g.drawText(item.label, swatch, Justification::centred);
    });
}

int PsgClipHeader::getLegendWidth() {
    return getPsgLegendWidth();
}

void PsgClipHeader::paintLegend(Graphics& g, Rectangle<int> area) {
    paintPsgLegend(g, area);
}

//==============================================================================
String getPsgSetInfo(te::Edit& edit) {
    Array<PsgClip*> clips;
    for (auto* track : te::getAudioTracks(edit))
        for (auto* clip : track->getClips())
            if (auto* psgClip = dynamic_cast<PsgClip*>(clip))
                clips.add(psgClip);

    if (clips.size() <= 1)
        return clips.isEmpty() ? String() : getPsgClipInfo(*clips[0]);

    // ponytail: detected together, the clips' own tuning choices are not consulted;
    // a set-wide choice needs clips linked in the model.
    const auto detection = detectTuningTogether(clips);
    if (! detection.hasNotes)
        return {};
    auto text = detection.getTableLabel() + "  " + detection.keys.front().getName();
    if (detection.tableTie || isKeyTie(detection.keys))
        text << " ?";
    return text;
}

PsgLegendComponent::PsgLegendComponent() {
    setInterceptsMouseClicks(false, false);
    setSize(getPsgLegendWidth() + 2, 14);
}

void PsgLegendComponent::paint(Graphics& g) {
    // The envelope and noise swatches are see-through, drawn for the dark body.
    g.setColour(Colors::Theme::backgroundAlt);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 2.0f);
    paintPsgLegend(g, getLocalBounds().reduced(1));
}

PsgSetInfoComponent::PsgSetInfoComponent(te::Edit& edit)
    : edit_(edit)
{
    setInterceptsMouseClicks(false, false);
    edit_.state.addListener(this);
    triggerAsyncUpdate();  // the clips may come after this
}

PsgSetInfoComponent::~PsgSetInfoComponent() {
    edit_.state.removeListener(this);
}

void PsgSetInfoComponent::paint(Graphics& g) {
    g.setColour(Colors::Theme::textPrimary);
    g.setFont(FontOptions(11.0f));
    g.drawText(text_, getLocalBounds().reduced(ClipHeader::pad * 3, 0), Justification::centredLeft, true);
}

void PsgSetInfoComponent::handleAsyncUpdate() {
    if (auto text = getPsgSetInfo(edit_); text != text_) {
        text_ = text;
        repaint();
    }
}

void PsgSetInfoComponent::valueTreeChildAdded(juce::ValueTree&, juce::ValueTree& child) {
    if (te::Clip::isClipState(child))
        triggerAsyncUpdate();
}

void PsgSetInfoComponent::valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree& child, int) {
    if (te::Clip::isClipState(child))
        triggerAsyncUpdate();
}

void PsgSetInfoComponent::valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier& property) {
    if (isTuningProperty(property))
        triggerAsyncUpdate();
}

}  // namespace MoTool
