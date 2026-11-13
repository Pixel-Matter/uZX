#include "NoteLanes.h"

#include <cmath>

namespace MoTool {

namespace {

constexpr auto noteLanesSetting = "showNoteLanes";
constexpr auto pianoKeysSetting = "noteLanesAsPianoKeys";

// A line under every lane needs a few pixels, names about a line of small text. When a single
// lane is too short, one per octave can still be shown, down to a few pixels an octave: the line
// under each C, and the tonic's name. Piano keys need lanes tall enough to tell apart.
constexpr float minLaneHeight = 4.0f, minOctaveHeight = 3.0f, minLabelHeight = 8.0f, minPianoKeyHeight = 4.0f;

// A black key holds a sharp's name, C#4; right of it, a white key holds its own.
constexpr float whiteKeyWidth = 32.0f, blackKeyWidth = 18.0f, keyNameGap = 2.0f;
constexpr float laneLabelGap = 3.0f, laneLabelWidth = 40.0f;

/** Piano Keys mode: a short keyboard over the grid at the view's left edge, just enough to
    show the groups of two and three black keys that tell where C is. Each lane is one key;
    white keys meet halfway along the black key between them, as on a piano. */
void paintPianoKeys(Graphics& g, const std::vector<NoteLane>& lanes, const NotesLayout& layout) {
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

}  // namespace

bool areNoteLanesShown(tracktion::Engine& engine) {
    return engine.getPropertyStorage().getPropertiesFile().getBoolValue(noteLanesSetting, true);
}

void setNoteLanesShown(tracktion::Engine& engine, bool shouldShow) {
    engine.getPropertyStorage().getPropertiesFile().setValue(noteLanesSetting, shouldShow);
}

bool arePianoKeysShown(tracktion::Engine& engine) {
    return engine.getPropertyStorage().getPropertiesFile().getBoolValue(pianoKeysSetting, false);
}

void setPianoKeysShown(tracktion::Engine& engine, bool shouldShow) {
    engine.getPropertyStorage().getPropertiesFile().setValue(pianoKeysSetting, shouldShow);
}

LaneDetail getLaneDetail(float noteHeight) {
    auto level = [noteHeight](float minLane, float minOctave) {
        return noteHeight >= minLane             ? LaneDetail::All
               : noteHeight * 12.0f >= minOctave ? LaneDetail::Tonic
                                                 : LaneDetail::None;
    };
    return {level(minLaneHeight, minOctaveHeight), level(minLabelHeight, minLabelHeight)};
}

float LaneView::getLabelX() const {
    return layout.visibleX.getStart() + (pianoKeys ? blackKeyWidth + keyNameGap : laneLabelGap);
}

float LaneView::getGutterRight() const {
    if (pianoKeys)
        return layout.visibleX.getStart() + whiteKeyWidth;
    return getLabelX() + (detail.labels != LaneDetail::None ? laneLabelWidth : 0.0f);
}

Range<int> LaneView::getViewX() const {
    return {static_cast<int>(std::floor(layout.visibleX.getStart())),
            static_cast<int>(std::ceil(layout.visibleX.getEnd()))};
}

int LaneView::getGridX() const {
    const auto viewX = getViewX();
    if (detail.labels == LaneDetail::None && !pianoKeys)
        return viewX.getStart();
    return jlimit(viewX.getStart(), viewX.getEnd(), static_cast<int>(std::ceil(getGutterRight())));
}

std::optional<LaneView> makeLaneView(tracktion::Engine& engine, const NotesLayout& layout) {
    if (!areNoteLanesShown(engine))
        return std::nullopt;
    auto detail = getLaneDetail(layout.noteHeight);
    if (detail.lines == LaneDetail::None)
        return std::nullopt;
    const bool pianoKeys = arePianoKeysShown(engine) && layout.noteHeight >= minPianoKeyHeight;
    // A tonic's name alone would spill over the keys around it, which show where C is anyway.
    if (pianoKeys && detail.labels == LaneDetail::Tonic)
        detail.labels = LaneDetail::None;
    return LaneView{layout, detail, pianoKeys};
}

void paintNoteLanes(Graphics& g,
                    const LaneView& view,
                    const std::vector<NoteLane>& lanes,
                    const std::optional<LaneKey>& key,
                    Colour background) {
    const auto& layout = view.layout;
    const auto detail = view.detail;
    const bool pianoKeys = view.pianoKeys;

    // Without a key the octave anchor falls back to C and no lane is shaded; the keys name C major.
    const int tonic = key ? key->tonic : 0;
    const Scale scale(key ? key->scale : Scale::ScaleType(Scale::ScaleType::IonianOrMajor));
    const float x = layout.visibleX.getStart(), right = layout.visibleX.getEnd();
    const float gridX = pianoKeys ? x + whiteKeyWidth : x;  // the keys take the grid's left end
    const float labelX = view.getLabelX(), labelWidth = view.getGutterRight() - labelX;
    const auto damage = g.getClipBounds().toFloat();
    constexpr float tonicLabelHeight = 10.0f;
    // As the timeline grid's finest and medium ticks look on the clip
    const auto laneLine = background.overlaidWith(Colors::Timeline::trackGridTickColors[0]);
    const auto octaveLine = background.overlaidWith(Colors::Timeline::trackGridTickColors[1]);
    const bool labelsInDamage =
        detail.labels != LaneDetail::None && damage.getX() < labelX + labelWidth && damage.getRight() > x;
    if (labelsInDamage)
        g.setFont(Font(FontOptions(detail.labels == LaneDetail::All ? jmin(11.0f, layout.noteHeight - 2.0f) : 9.0f)));
    if (pianoKeys)
        paintPianoKeys(g, lanes, layout);

    for (const auto& lane : lanes) {
        const float top = layout.normToY(lane.hi), bottom = layout.normToY(lane.lo);
        // A tonic label hangs above the lane's bottom boundary and may overflow a short lane.
        if (bottom < damage.getY() || jmin(top, bottom - tonicLabelHeight) > damage.getBottom())
            continue;

        const int degree = ((lane.note - tonic) % 12 + 12) % 12;
        const bool isTonic = degree == 0;
        if (key && isTonic && detail.lines == LaneDetail::All && right > gridX) {
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
        if (!labelsInDamage)
            continue;
        if (detail.labels == LaneDetail::All) {
            // Over the keys only the key's notes are named, each on its own key: light at a black
            // key's left end, dark right of the black keys on a white one.
            if (pianoKeys && !scale.isIntervalInScale(degree))
                continue;
            const bool onBlackKey = pianoKeys && MidiMessage::isMidiNoteBlack(lane.note);
            const float nameX = onBlackKey ? x : labelX;
            const auto ink = pianoKeys && !onBlackKey ? Colours::black : Colours::white;
            g.setColour(ink.withAlpha(isTonic ? 0.8f : 0.45f));
            g.drawText(lane.label,
                       Rectangle<float>(nameX, top, labelX + labelWidth - nameX, bottom - top),
                       Justification::centredLeft,
                       false);
        } else if (isTonic) {
            g.setColour(Colours::white.withAlpha(0.45f));
            g.drawText(lane.label,
                       Rectangle<float>(labelX, bottom - tonicLabelHeight, labelWidth, tonicLabelHeight),
                       Justification::bottomLeft,
                       false);
        }
    }
}

void paintTicksOverLanes(Graphics& g, const LaneView& view, TimelineGrid& grid, int clipX, Colour background) {
    const Range<int> gridX{view.getGridX(), view.getViewX().getEnd()};
    for (const auto& tick : grid.getTicks()) {
        const int x = tick.x - clipX;  // ticks are in the track body's x
        if (tick.level == 0 || tick.level >= Colors::Timeline::trackGridTickColors.size() || !gridX.contains(x))
            continue;
        g.setColour(background.overlaidWith(Colors::Timeline::trackGridTickColors[tick.level]));
        g.drawVerticalLine(x, 0.0f, view.layout.height);
    }
}

}  // namespace MoTool
