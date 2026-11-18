#include "TempoInspector.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <span>

#include "../../models/EditUtilities.h"
#include "../../models/PsgClip.h"
#include "../../models/PsgFrameRetimer.h"
#include "../../models/PsgRowEvidence.h"
#include "../../models/PsgTempoDetector.h"
#include "../common/LookAndFeel.h"
#include "../common/Utilities.h"

namespace MoTool {

namespace {

using Theme = Colors::Theme;
using Analysis = PsgTempo::Analysis;

constexpr int mapWindow = 256;                // frames one column of the grid map folds
constexpr int mapHop = 128;                   // frames from one column to the next
constexpr int minPeriod = 2, maxPeriod = 40;  // the grid map's rows: every row grid, and every beat in the BPM range
constexpr float minMapScale = 0.1f;           // the least share the grid map's brightest colour stands for
constexpr int minMapEvents = 6;  // frames with row evidence a column needs: fewer lie on any grid by chance
constexpr float stripFrameWidth = 3.0f;
constexpr int gutter = 44;  // left of every plot, for its labels, so the plots line up
constexpr int rowHeight = 18;

juce::Font smallFont() {
    return juce::Font(juce::FontOptions(11.0f));
}

juce::String timeText(double seconds) {
    const int whole = (int) seconds;
    return juce::String(whole / 60) + ":" + juce::String(whole % 60).paddedLeft('0', 2);
}

juce::String rowsText(const std::vector<int>& rows) {
    juce::StringArray lengths;
    for (int row : rows)
        lengths.add(juce::String(row));
    return lengths.joinIntoString("-");
}

juce::String scoreText(const juce::Optional<double>& score) {
    return score.hasValue() ? juce::String(*score, 2) : juce::String("-");
}

/** A share of 0..1 as a colour, from the panel's background to the accent; low shares, which any
    grid gets by chance, stay dark. */
juce::Colour heat(double share) {
    return Theme::backgroundAlt.interpolatedWith(Theme::primary, (float) std::pow(juce::jlimit(0.0, 1.0, share), 1.5));
}

/** What a frame's event weighs: for a row start in the warm colour, against, fainter, in the cool one. */
juce::Colour weightColour(double weight) {
    return weight >= 0.0
               ? Theme::backgroundAlt.interpolatedWith(Theme::warning, (float) juce::jmin(1.0, weight / 6.0))
               : Theme::backgroundAlt.interpolatedWith(Theme::secondary, (float) juce::jmin(0.4, -weight / 10.0));
}

//==============================================================================
/** What the inspector shows: a tune's analysis, and where the user looks at it. */
struct Inspection
{
    Analysis analysis;
    std::vector<PsgRowEvidence::FrameWeights> weights;  // of the clip the inspector was opened on
    double fps = 50.0;                                  // of the clip's frames
    int lag = 24;                                       // the beat cursor, in frames
    int lagPhase = 0;                                   // the frame of every `lag` that holds most row evidence
    int viewFrame = 0;                                  // the first frame the evidence strip shows
    int playFrame = -1;                                 // under the playhead
    std::function<void()> lagChanged;
    std::function<void(int)> seek;                 // moves the playhead to a frame
    std::function<void(const juce::String&)> say;  // what is under the mouse

    int numFrames() const { return (int) analysis.evidence.size(); }
    int maxLag() const {
        return analysis.repeat.empty() ? 2 * PsgTempo::maxBeatFrames : (int) analysis.repeat.size() - 1;
    }
    double bpmOf(int frames) const { return fps * 60.0 / juce::jmax(1, frames); }

    void setLag(int frames) {
        lag = juce::jlimit(minPeriod, maxLag(), frames);
        std::vector<double> byPhase((size_t) lag, 0.0);
        for (size_t i = 0; i < analysis.evidence.size(); ++i)
            byPhase[i % (size_t) lag] += analysis.evidence[i];
        lagPhase = (int) std::distance(byPhase.begin(), std::max_element(byPhase.begin(), byPhase.end()));
        if (lagChanged)
            lagChanged();
    }

    /** Whether the detection's own row grid is this one. */
    bool isDetected(const Analysis::Grid& grid) const {
        return analysis.rowsFrom == Analysis::RowsFrom::evidence && analysis.result
               && analysis.result->rowFrames == grid.rows;
    }
};

/** A panel of the inspector: its plot is right of the gutter. */
class Panel : public juce::Component
{
public:
    explicit Panel(Inspection& inspection) : in(inspection) {}

protected:
    juce::Rectangle<float> plotArea(int top = 0, int bottom = 0) const {
        return getLocalBounds().withTrimmedLeft(gutter).withTrimmedTop(top).withTrimmedBottom(bottom).toFloat();
    }

    void label(juce::Graphics& g,
               const juce::String& text,
               float centreY,
               juce::Colour colour = Theme::textSecondary) const {
        g.setColour(colour);
        g.setFont(smallFont());
        g.drawText(text, 0, juce::roundToInt(centreY) - 7, gutter - 6, 14, juce::Justification::centredRight);
    }

    Inspection& in;
};

//==============================================================================
/** Where a row grid holds: a column per stretch of the tune, a row per grid period, brighter the
    more of the stretch's row evidence lies on one phase of that period. A tune at one speed is a
    bright line across, with its multiples under it; a slip, a change of speed or a groove breaks it.
    The colours run up to the map's own brightest cell, named under it, so the faint line of a
    groove of many rows, which no one phase holds much of, shows too. */
class GridMap : public Panel
{
public:
    explicit GridMap(Inspection& inspection) : Panel(inspection) {
        const std::span<const double> evidence(in.analysis.evidence);
        for (size_t start = 0; start < evidence.size(); start += mapHop) {
            const auto stretch = evidence.subspan(start, std::min<size_t>(mapWindow, evidence.size() - start));
            const auto events = std::count_if(stretch.begin(), stretch.end(), [](double e) { return e > 0.0; });
            auto& column = shares.emplace_back();
            for (int period = minPeriod; period <= maxPeriod; ++period)
                column.push_back(events < minMapEvents ? 0.0f : (float) PsgTempo::gridShare(stretch, period));
            scale = std::max(scale, *std::max_element(column.begin(), column.end()));
            if (start + mapWindow >= evidence.size())
                break;
        }
    }

    void paint(juce::Graphics& g) override {
        const auto plot = plotArea(0, 16);
        if (shares.empty())
            return;
        const float cw = columnWidth(), ch = plot.getHeight() / (float) (maxPeriod - minPeriod + 1);
        for (size_t c = 0; c < shares.size(); ++c)
            for (size_t p = 0; p < shares[c].size(); ++p) {
                g.setColour(heat(shares[c][p] / scale));
                g.fillRect(plot.getX() + (float) c * cw,
                           plot.getY() + (float) p * ch,
                           juce::jmax(1.0f, cw - 1.0f),
                           juce::jmax(1.0f, ch - 1.0f));
            }

        auto rowY = [&](int period) { return plot.getY() + (float) (period - minPeriod) * ch; };
        for (int period = minPeriod; period <= maxPeriod; ++period)
            if (period == in.lag)
                label(g, juce::String(period), rowY(period) + ch / 2, Theme::textPrimary);
            else if (period % 4 == 0 && std::abs(period - in.lag) > 1)
                label(g, juce::String(period), rowY(period) + ch / 2);
        if (in.lag <= maxPeriod) {
            g.setColour(Theme::textPrimary);
            g.drawRect(juce::Rectangle<float>(plot.getX(), rowY(in.lag), plot.getWidth(), ch).expanded(1.0f), 1.0f);
        }

        g.setColour(Theme::textSecondary);
        g.setFont(smallFont());
        const int scaleWidth = 110;  // the right end of the axis names the scale
        const auto step = (size_t) juce::jmax(1, (int) std::ceil(60.0f / cw));
        for (size_t c = 0; c < shares.size(); c += step)
            if (const int x = juce::roundToInt(plot.getX() + (float) c * cw);
                x + 40 < (int) plot.getRight() - scaleWidth)
                g.drawText(timeText((double) (c * mapHop) / in.fps),
                           x,
                           (int) plot.getBottom() + 2,
                           60,
                           14,
                           juce::Justification::centredLeft);
        g.drawText("Brightest: " + juce::String(scale, 2),
                   (int) plot.getRight() - scaleWidth,
                   (int) plot.getBottom() + 2,
                   scaleWidth,
                   14,
                   juce::Justification::centredRight);

        // the frames the strip shows, and the playhead
        g.setColour(Theme::textSecondary);
        g.fillRect(xOf(in.viewFrame),
                   plot.getBottom(),
                   juce::jmax(2.0f, xOf(in.viewFrame + stripFrames) - xOf(in.viewFrame)),
                   2.0f);
        if (in.playFrame >= 0 && in.playFrame < in.numFrames()) {
            g.setColour(Theme::highlight);
            g.fillRect(xOf(in.playFrame), plot.getY(), 1.0f, plot.getHeight());
        }
    }

    void mouseDown(const juce::MouseEvent& e) override { mouseDrag(e); }
    void mouseDrag(const juce::MouseEvent& e) override { in.seek(frameAt(e.position.x)); }

    void mouseMove(const juce::MouseEvent& e) override {
        const auto plot = plotArea(0, 16);
        const int column = frameAt(e.position.x) / mapHop;
        const int period =
            minPeriod + (int) ((e.position.y - plot.getY()) / plot.getHeight() * (float) (maxPeriod - minPeriod + 1));
        if (!plot.contains(e.position) || column >= (int) shares.size() || period > maxPeriod)
            return;
        in.say("At " + timeText(column * mapHop / in.fps) + ", "
               + juce::String(shares[(size_t) column][(size_t) (period - minPeriod)], 2)
               + " of the row evidence lies on one grid of " + juce::String(period) + " frames. A row grid needs "
               + juce::String(PsgTempo::minGridShare, 2) + " over the whole tune.");
    }

    int stripFrames = 0;  // how many frames the evidence strip shows, for its mark under the map

private:
    float columnWidth() const { return plotArea().getWidth() / (float) std::max<size_t>(1, shares.size()); }
    float xOf(int frame) const { return plotArea().getX() + (float) frame / (float) mapHop * columnWidth(); }
    int frameAt(float x) const {
        return juce::jlimit(
            0, juce::jmax(0, in.numFrames() - 1), (int) ((x - plotArea().getX()) / columnWidth() * (float) mapHop));
    }

    std::vector<std::vector<float>> shares;  // [column][period - minPeriod]
    float scale = minMapScale;               // the share the brightest colour stands for
};

//==============================================================================
/** The row evidence frame by frame, around the playhead: what each channel and the chip weigh for
    a row start, and their sum where it counts. Above it the rows the detection settled on, a
    longer tick where a beat starts; below it the beats of the cursor. */
class EvidenceStrip : public Panel
{
public:
    using Panel::Panel;

    int visibleFrames() const { return juce::jmax(1, (int) (plotArea().getWidth() / stripFrameWidth)); }

    void paint(juce::Graphics& g) override {
        const auto plot = plotArea(10, 10);
        const int first = in.viewFrame, count = visibleFrames();
        const float rh = plot.getHeight() / 5.0f;
        auto xOf = [&](int frame) { return plot.getX() + (float) (frame - first) * stripFrameWidth; };

        static const char* const names[] = {"A", "B", "C", "chip", "rows"};
        for (int row = 0; row < 5; ++row) {
            label(g, names[row], plot.getY() + ((float) row + 0.5f) * rh);
            for (int frame = juce::jmax(0, first); frame < first + count; ++frame) {
                const auto at = (size_t) frame;
                if (row < 4 ? at >= in.weights.size() : at >= in.analysis.evidence.size())
                    break;
                g.setColour(row < 4 ? weightColour(in.weights[at][(size_t) row])
                                    : heat(in.analysis.evidence[at] / 8.0));
                g.fillRect(xOf(frame), plot.getY() + (float) row * rh, stripFrameWidth, rh - 1.0f);
            }
        }

        if (const auto& result = in.analysis.result) {
            g.setColour(Theme::textPrimary);
            const auto& rows = result->rowFrames;
            for (int frame = result->gridPhase, row = 0; frame < first + count;
                 frame += rows[(size_t) row++ % rows.size()])
                if (frame >= first) {
                    const float height = row % result->rowsPerBeat == 0 ? 8.0f : 4.0f;
                    g.fillRect(xOf(frame), plot.getY() - height - 1.0f, 1.0f, height);
                }
        }
        g.setColour(Theme::textSecondary);
        for (int frame = first - first % in.lag + in.lagPhase - in.lag; frame < first + count; frame += in.lag)
            if (frame >= first)
                g.fillRect(xOf(frame), plot.getBottom() + 1.0f, 1.0f, 8.0f);

        if (in.playFrame >= first && in.playFrame < first + count) {
            g.setColour(Theme::highlight);
            g.fillRect(xOf(in.playFrame), 0.0f, 1.0f, (float) getHeight());
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        in.seek(juce::jlimit(0,
                             juce::jmax(0, in.numFrames() - 1),
                             in.viewFrame + (int) ((e.position.x - plotArea().getX()) / stripFrameWidth)));
    }

    void mouseMove(const juce::MouseEvent&) override {
        in.say("Ticks above: the rows detected, longer where a beat starts. Ticks below: the cursor's beats. "
               "Click to move the playhead.");
    }
};

//==============================================================================
/** How alike the row evidence is to itself by lag: its autocorrelation, with the lags the BPM range
    allows a beat shaded and the least a pulse's beat must repeat drawn across. The beat cursor is
    set here; its doublings are marked, since a beat is a pulse times a power of two. */
class RepeatPlot : public Panel
{
public:
    using Panel::Panel;

    void paint(juce::Graphics& g) override {
        const auto plot = plotArea(4, 16);
        const auto& repeat = in.analysis.repeat;
        if (repeat.size() < 3) {
            g.setColour(Theme::textSecondary);
            g.setFont(smallFont());
            g.drawText("The tune is too short to tell what repeats", plot, juce::Justification::centred);
            return;
        }
        const double top = juce::jmax(0.5, *std::max_element(repeat.begin() + 2, repeat.end()));
        const double bottom = juce::jmin(-0.1, *std::min_element(repeat.begin() + 1, repeat.end()));
        auto xOf = [&](int lag) {
            return plot.getX() + (float) (lag - 1) / (float) (in.maxLag() - 1) * plot.getWidth();
        };
        auto yOf = [&](double value) {
            return plot.getBottom() - (float) ((value - bottom) / (top - bottom)) * plot.getHeight();
        };

        g.setColour(Theme::backgroundAlt);
        g.fillRect(juce::Rectangle<float>::leftTopRightBottom(
            xOf(PsgTempo::minBeatFrames), plot.getY(), xOf(PsgTempo::maxBeatFrames), plot.getBottom()));
        g.setColour(Theme::border);
        g.fillRect(plot.getX(), yOf(0.0), plot.getWidth(), 1.0f);
        const float dashes[] = {3.0f, 3.0f};
        g.drawDashedLine(
            {plot.getX(), yOf(PsgTempo::minBeatRepeat), plot.getRight(), yOf(PsgTempo::minBeatRepeat)}, dashes, 2);
        label(g, "0", yOf(0.0));
        label(g, juce::String(PsgTempo::minBeatRepeat, 1), yOf(PsgTempo::minBeatRepeat));

        for (int times : {1, 2, 4})
            if (in.lag * times <= in.maxLag()) {
                g.setColour(times == 1 ? Theme::textPrimary : Theme::textSecondary);
                g.fillRect(xOf(in.lag * times), plot.getY(), 1.0f, plot.getHeight());
            }

        juce::Path curve;
        for (int lag = 1; lag <= in.maxLag(); ++lag)
            if (lag == 1)
                curve.startNewSubPath(xOf(lag), yOf(repeat[(size_t) lag]));
            else
                curve.lineTo(xOf(lag), yOf(repeat[(size_t) lag]));
        g.setColour(Theme::primary);
        g.strokePath(curve, juce::PathStrokeType(1.5f));

        // the beat the detection settled on
        if (const auto& result = in.analysis.result; result && result->framesPerBeat <= in.maxLag()) {
            g.setColour(Theme::highlight);
            const float x = xOf(result->framesPerBeat);
            juce::Path mark;
            mark.addTriangle(x - 4.0f, plot.getY() - 4.0f, x + 4.0f, plot.getY() - 4.0f, x, plot.getY() + 3.0f);
            g.fillPath(mark);
        }

        g.setColour(Theme::textSecondary);
        g.setFont(smallFont());
        for (int lag = 20; lag <= in.maxLag(); lag += 20)
            g.drawText(juce::String(lag),
                       juce::roundToInt(xOf(lag)) - 20,
                       (int) plot.getBottom() + 2,
                       40,
                       14,
                       juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& e) override { mouseDrag(e); }

    /** The cursor goes to the lag under the mouse, or to a peak within two frames of it. */
    void mouseDrag(const juce::MouseEvent& e) override {
        const auto plot = plotArea(4, 16);
        const auto& repeat = in.analysis.repeat;
        int lag = juce::jlimit(
            minPeriod,
            in.maxLag(),
            1 + juce::roundToInt((e.position.x - plot.getX()) / plot.getWidth() * (float) (in.maxLag() - 1)));
        const int under = lag;
        for (int other = juce::jmax(minPeriod, under - 2); other <= juce::jmin(in.maxLag(), under + 2); ++other)
            if ((size_t) other < repeat.size() && (size_t) lag < repeat.size()
                && repeat[(size_t) other] > repeat[(size_t) lag])
                lag = other;
        in.setLag(lag);
    }

    void mouseMove(const juce::MouseEvent&) override {
        in.say("How alike the row evidence is to itself, by lag in frames. Shaded: the beats the BPM range allows. "
               "Dashed: the "
               "least a beat must repeat when no row grid holds. Click or drag to set the beat cursor.");
    }
};

//==============================================================================
/** The share of the whole tune's row evidence on each uniform row grid, against the share a grid needs. */
class GridBars : public Panel
{
public:
    explicit GridBars(Inspection& inspection) : Panel(inspection) {
        for (const auto& grid : in.analysis.grids)
            if (grid.rows.size() == 1)
                uniform.push_back(grid);
    }

    void paint(juce::Graphics& g) override {
        const auto plot = plotArea(4, 16);
        if (uniform.empty()) {
            g.setColour(Theme::textSecondary);
            g.setFont(smallFont());
            g.drawText("Too little row evidence for a grid", plot, juce::Justification::centred);
            return;
        }
        const float bw = plot.getWidth() / (float) uniform.size();
        auto yOf = [&](double share) {
            return plot.getBottom() - (float) juce::jlimit(0.0, 1.0, share) * plot.getHeight();
        };
        g.setFont(smallFont());
        for (size_t i = 0; i < uniform.size(); ++i) {
            const auto& grid = uniform[i];
            const float x = plot.getX() + (float) i * bw;
            g.setColour(in.isDetected(grid)      ? Theme::primary
                        : grid.rows[0] == in.lag ? Theme::surfaceElevated
                                                 : Theme::surface);
            g.fillRect(x + 1.0f, yOf(grid.share), bw - 2.0f, plot.getBottom() - yOf(grid.share));
            g.setColour(grid.rows[0] == in.lag ? Theme::textPrimary : Theme::textSecondary);
            g.drawText(juce::String(grid.rows[0]),
                       juce::Rectangle<float>(x - 4.0f, plot.getBottom() + 2.0f, bw + 8.0f, 14.0f),
                       juce::Justification::centred);
        }
        g.setColour(Theme::border);
        const float dashes[] = {3.0f, 3.0f};
        g.drawDashedLine(
            {plot.getX(), yOf(PsgTempo::minGridShare), plot.getRight(), yOf(PsgTempo::minGridShare)}, dashes, 2);
        label(g, juce::String(PsgTempo::minGridShare, 1), yOf(PsgTempo::minGridShare));
    }

    void mouseDown(const juce::MouseEvent& e) override {
        if (const auto* grid = gridAt(e.position.x))
            in.setLag(grid->rows[0]);
    }

    void mouseMove(const juce::MouseEvent& e) override {
        if (const auto* grid = gridAt(e.position.x))
            in.say("Rows of " + juce::String(grid->rows[0]) + " frames hold " + juce::String(grid->share, 2)
                   + " of the whole tune's row evidence; a grid needs " + juce::String(PsgTempo::minGridShare, 2)
                   + ". Click to put the cursor there.");
    }

private:
    const Analysis::Grid* gridAt(float x) const {
        const auto plot = plotArea(4, 16);
        const int index = (int) ((x - plot.getX()) / plot.getWidth() * (float) uniform.size());
        return x >= plot.getX() && index >= 0 && index < (int) uniform.size() ? &uniform[(size_t) index] : nullptr;
    }

    std::vector<Analysis::Grid> uniform;
};

//==============================================================================
/** The beats the detection weighed, and the cursor's: a click on one puts the cursor on it. */
class BeatTable : public Panel
{
public:
    using Panel::Panel;

    int idealHeight() const { return rowHeight * ((int) in.analysis.beats.size() + 2); }

    void paint(juce::Graphics& g) override {
        static const char* const headers[] = {"Beat", "Frames", "BPM", "Drums and bass", "Accents", "Evidence"};
        g.setFont(smallFont());
        g.setColour(Theme::textSecondary);
        for (int column = 0; column < 6; ++column)
            g.drawText(headers[column], cell(0, column), juce::Justification::centredLeft);

        const auto& beats = in.analysis.beats;
        const bool pulses = in.analysis.pulse > 0;
        for (size_t i = 0; i <= beats.size(); ++i) {
            const bool isCursor = i == beats.size();
            const auto beat =
                isCursor ? Analysis::Beat{0, in.lag, {}, {}, PsgTempo::repeatOverBeats(in.analysis.repeat, in.lag)}
                         : beats[i];
            const bool chosen = !isCursor && in.analysis.result && in.analysis.result->framesPerBeat == beat.frames;
            // a beat found by following the peaks is not a whole number of pulses
            const bool about = pulses && beat.frames != beat.rowsPerBeat * in.analysis.pulse;
            const int row = (int) i + 1;
            if (!isCursor && beat.frames == in.lag) {
                g.setColour(Theme::backgroundSel);
                g.fillRect(cell(row, 0).withRight((float) getWidth()));
            }
            g.setColour(chosen || isCursor ? Theme::textPrimary : Theme::textSecondary);
            const juce::String texts[] = {isCursor
                                              ? juce::String("Cursor")
                                              : juce::String(about ? "About " : "") + juce::String(beat.rowsPerBeat)
                                                    + (pulses ? " pulse" : " row") + (beat.rowsPerBeat == 1 ? "" : "s")
                                                    + (chosen ? ", detected" : ""),
                                          juce::String(beat.frames),
                                          juce::String(in.bpmOf(beat.frames), 1),
                                          scoreText(beat.patterns),
                                          scoreText(beat.accents),
                                          scoreText(beat.repeat)};
            for (int column = 0; column < 6; ++column)
                g.drawText(texts[column], cell(row, column), juce::Justification::centredLeft);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        const int row = (int) e.position.y / rowHeight - 1;
        if (row >= 0 && row < (int) in.analysis.beats.size())
            in.setLag(in.analysis.beats[(size_t) row].frames);
    }

    void mouseMove(const juce::MouseEvent&) override {
        in.say("Drums and bass: share of rows that sound like the row 2, 4 and 8 beats before. Accents: how the rows' "
               "loudness recurs a beat on. Evidence: how alike the row evidence is 1, 2 and 4 beats on. Click a beat "
               "to put the cursor on it.");
    }

private:
    juce::Rectangle<float> cell(int row, int column) const {
        const float width = (float) (getWidth() - gutter) / 6.0f;
        return {(float) gutter + (float) column * width, (float) (row * rowHeight), width, (float) rowHeight};
    }
};

//==============================================================================
/** How the detection came to its answer, in words. */
juce::String describe(const Inspection& in) {
    using RowsFrom = Analysis::RowsFrom;
    const auto& a = in.analysis;
    juce::StringArray lines;

    if (const auto& r = a.result)
        lines.add(juce::String(r->bpm, 2) + " BPM: "
                  + (r->rowsArePulse && r->rowsPerBeat == 1
                         ? juce::String("a beat of ")
                         : (r->rowsArePulse ? "a pulse of " : "rows of ") + rowsText(r->rowFrames) + " frames, "
                               + juce::String(r->rowsPerBeat) + " to a beat of ")
                  + juce::String(r->framesPerBeat) + " frames, " + juce::String(r->beatsPerBar) + "/4.");
    else
        lines.add("No tempo detected.");

    const Analysis::Grid* best = nullptr;
    juce::StringArray patterns;
    for (const auto& grid : a.grids) {
        if (grid.rows.size() == 1 && (best == nullptr || grid.share > best->share))
            best = &grid;
        if (grid.rows.size() > 1)
            patterns.add(rowsText(grid.rows) + " holds " + juce::String(grid.share, 2));
    }
    const auto needs = " (a grid needs " + juce::String(PsgTempo::minGridShare, 2) + ")";
    juce::String rows;
    if (a.rowsFrom == RowsFrom::evidence)
        rows = "Rows: a grid of the row evidence, holding " + juce::String(a.result->gridShare, 2) + " of it" + needs
               + ".";
    else if (best == nullptr)
        rows = "Rows: too little row evidence for a grid.";
    else
        rows = "Rows: no grid holds the row evidence; the best, " + juce::String(best->rows[0]) + " frames, holds "
               + juce::String(best->share, 2) + needs + ".";
    if (a.rowsFrom == RowsFrom::noteStarts)
        rows << " A grid of the plain note starts does, holding " << juce::String(a.result->gridShare, 2) << ".";
    else if (a.rowsFrom == RowsFrom::pulse)
        rows << " The evidence repeats every " << juce::String(a.pulse)
             << (a.result->rowFrames[0] == a.pulse
                     ? juce::String(" frames: that pulse stands for the rows.")
                     : " frames, but not in whole frames: its doublings drift off where the tune repeats, so the beat "
                       "follows the peaks to "
                           + juce::String(a.result->framesPerBeat) + " frames.");
    else if (a.rowsFrom == RowsFrom::nothing && a.pulse > 0)
        rows << " It repeats every " << juce::String(a.pulse) << " frames, but no beat of that pulse repeats "
             << juce::String(PsgTempo::minBeatRepeat, 2) << " over 1, 2 and 4 beats.";
    else if (a.rowsFrom == RowsFrom::nothing && best != nullptr)
        rows << (a.repeat.empty() ? " The tune is too short to tell what repeats."
                                  : " Nothing repeats clearly enough for a pulse.");
    if (!patterns.isEmpty())
        rows << " Best swing or groove: " << patterns.joinIntoString(", ") << ".";
    lines.add(rows);

    if (const auto& r = a.result) {
        const bool triple = a.meterMargin > 0.0;
        lines.add(juce::String("Meter: ") + (r->meterClear ? (triple ? "3/4" : "4/4") : "unclear, left as it is")
                  + ". Bars of "
                  + (triple ? "3 beats repeat better than bars of 4" : "4 beats repeat better than bars of 3") + " by "
                  + juce::String(std::abs(a.meterMargin), 2) + " (clear from "
                  + juce::String(PsgTempo::minMeterMargin, 2) + ").");
    }
    return lines.joinIntoString("\n");
}

//==============================================================================
class TempoInspector : public juce::Component, private juce::Timer
{
public:
    explicit TempoInspector(PsgClip& clip)
        : clip_(&clip), clickWasOn_(clip.edit.clickTrackEnabled.get()), map_(in), strip_(in), repeat_(in), bars_(in),
          table_(in) {
        setWantsKeyboardFocus(true);
        for (auto* panel : std::initializer_list<juce::Component*>{&map_, &strip_, &repeat_, &bars_, &table_})
            addAndMakeVisible(panel);
        for (auto* button : std::initializer_list<juce::Button*>{&play_, &click_, &apply_, &copy_}) {
            button->setWantsKeyboardFocus(false);
            addAndMakeVisible(button);
        }
        for (auto* text : {&trace_, &cursor_, &status_}) {
            text->setFont(smallFont());
            text->setMinimumHorizontalScale(1.0f);
            addAndMakeVisible(text);
        }
        trace_.setJustificationType(juce::Justification::topLeft);
        trace_.setText(describe(in), juce::dontSendNotification);
        status_.setColour(juce::Label::textColourId, Theme::textSecondary);
        status_.setJustificationType(juce::Justification::topLeft);

        click_.setToggleState(clickWasOn_, juce::dontSendNotification);
        click_.onClick = [this] {
            if (auto* c = clip_.get())
                c->edit.clickTrackEnabled = click_.getToggleState();
        };
        play_.onClick = [this] { togglePlay(); };
        apply_.onClick = [this] { applyCursorBeat(); };
        copy_.onClick = [this] {
            if (auto* c = clip_.get())
                juce::SystemClipboard::copyTextToClipboard(c->getName().quoted() + ","
                                                           + juce::String(in.bpmOf(in.lag), 2));
        };

        in.lagChanged = [this] {
            cursor_.setText("Cursor: " + juce::String(in.lag) + " frames, " + juce::String(in.bpmOf(in.lag), 2)
                                + " BPM",
                            juce::dontSendNotification);
            repaint();
        };
        in.say = [this](const juce::String& text) { status_.setText(text, juce::dontSendNotification); };
        in.seek = [this](int frame) {
            if (auto* c = clip_.get())
                c->edit.getTransport().setPosition(timeOf(*c, frame));
        };
        const auto& result = in.analysis.result;
        in.setLag(result ? result->framesPerBeat : in.analysis.pulse > 0 ? in.analysis.pulse : 24);
        setSize(860, 720);
        startTimerHz(30);
    }

    ~TempoInspector() override {
        if (auto* c = clip_.get())
            c->edit.clickTrackEnabled = clickWasOn_;  // the click is the inspector's: a project doesn't keep it
    }

    void paint(juce::Graphics& g) override { g.fillAll(Theme::background); }

    void resized() override {
        auto area = getLocalBounds().reduced(10);
        auto buttons = area.removeFromTop(24);
        play_.setBounds(buttons.removeFromLeft(70));
        click_.setBounds(buttons.removeFromLeft(70).translated(8, 0));
        copy_.setBounds(buttons.removeFromRight(90));
        apply_.setBounds(buttons.removeFromRight(170).translated(-8, 0));
        cursor_.setBounds(buttons.withTrimmedLeft(16).withTrimmedRight(16));
        area.removeFromTop(6);
        trace_.setBounds(area.removeFromTop(64));  // four lines: the rows' line may wrap
        status_.setBounds(area.removeFromBottom(32));
        table_.setBounds(area.removeFromBottom(table_.idealHeight()));
        area.removeFromBottom(8);
        auto plots = area.removeFromBottom(150);
        bars_.setBounds(plots.removeFromRight(300));
        repeat_.setBounds(plots);
        area.removeFromBottom(8);
        strip_.setBounds(area.removeFromBottom(100));
        area.removeFromBottom(4);
        map_.setBounds(area);
        map_.stripFrames = strip_.visibleFrames();
    }

    bool keyPressed(const juce::KeyPress& key) override {
        if (key != juce::KeyPress::spaceKey)
            return false;
        togglePlay();
        return true;
    }

private:
    static std::vector<uZX::PsgData> dumpsOf(const juce::Array<PsgClip*>& clips) {
        std::vector<uZX::PsgData> dumps;
        for (auto* clip : clips)
            dumps.push_back(clip->getRegisterDump());
        return dumps;
    }

    /** The analysis of the clip's whole tune, as an import would detect it from the registers alone. */
    static Inspection inspect(PsgClip& clip) {
        const auto clips = clip.getTuneClips();
        const auto dumps = dumpsOf(clips);
        std::vector<const uZX::PsgData*> chips;
        for (const auto& dump : dumps)
            chips.push_back(&dump);
        Inspection inspection;
        inspection.analysis = PsgTempo::analyse(chips);
        inspection.weights = PsgRowEvidence::weightsOf(dumps[(size_t) juce::jmax(0, clips.indexOf(&clip))]);
        inspection.fps = dumps.front().getFrameRate();
        return inspection;
    }

    // ponytail: the transport's position, not the latency-aware playhead the timeline draws: a frame late at most
    tracktion::TimePosition timeOf(const PsgClip& clip, int frame) const {
        const auto position = clip.getPosition();
        return position.getStart() + tracktion::TimeDuration::fromSeconds(frame / in.fps) - position.getOffset();
    }

    int frameAt(const PsgClip& clip, tracktion::TimePosition time) const {
        const auto position = clip.getPosition();
        return (int) std::floor((time - position.getStart() + position.getOffset()).inSeconds() * in.fps);
    }

    void togglePlay() {
        if (auto* c = clip_.get()) {
            auto& transport = c->edit.getTransport();
            if (transport.isPlaying())
                transport.stop(false, false);
            else
                transport.play(false);
        }
    }

    /** The project's tempo becomes the cursor's beat, with the grid's usual groups, so its grid and
        click run against the tune. The frames keep their times. */
    void applyCursorBeat() {
        auto* c = clip_.get();
        if (c == nullptr)
            return;
        auto& edit = c->edit;
        const int framesPerBeat = juce::jmax(1, juce::roundToInt(in.lag * Helpers::getProjectFps(edit) / in.fps));
        PsgTiming::setFramesPerBeat(edit,
                                    edit.tempoSequence.getTempoAt(c->getPosition().getStart()),
                                    framesPerBeat,
                                    /*preserveAbsoluteFrameTimes=*/true);
        Helpers::setGridSubdivisionPattern(edit, Helpers::getDefaultGridSubdivisionPattern(framesPerBeat));
    }

    void timerCallback() override {
        auto* c = clip_.get();
        if (c == nullptr) {
            // the clip is gone, its edit maybe too: close, as launchAsync windows delete themselves on exit
            stopTimer();
            if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
                window->exitModalState(0);
            return;
        }
        auto& transport = c->edit.getTransport();
        play_.setButtonText(transport.isPlaying() ? "Stop" : "Play");
        const int frame = frameAt(*c, transport.getPosition());
        if (frame == in.playFrame)
            return;
        in.playFrame = frame;
        // the strip keeps the playhead a third of the way in
        in.viewFrame = juce::jmax(0, frame - strip_.visibleFrames() / 3);
        map_.repaint();
        strip_.repaint();
    }

    tracktion::SafeSelectable<PsgClip> clip_;
    const bool clickWasOn_;
    Inspection in{inspect(*clip_.get())};
    GridMap map_;
    EvidenceStrip strip_;
    RepeatPlot repeat_;
    GridBars bars_;
    BeatTable table_;
    juce::TextButton play_{"Play"}, apply_{"Set as project tempo"}, copy_{"Copy label"};
    juce::ToggleButton click_{"Click"};
    juce::Label trace_, cursor_, status_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TempoInspector)
};

}  // namespace

std::unique_ptr<juce::Component> createTempoInspector(PsgClip& clip) {
    return std::make_unique<TempoInspector>(clip);
}

void showTempoInspector(PsgClip& clip) {
    juce::DialogWindow::LaunchOptions o;
    o.dialogTitle = "Tempo inspector: " + clip.getName();
    o.dialogBackgroundColour = Theme::background;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.useBottomRightCornerResizer = true;
    o.content.setOwned(createTempoInspector(clip).release());
    Helpers::launchDialogAsync(o);
}

}  // namespace MoTool
