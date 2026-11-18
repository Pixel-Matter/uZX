#include "TempoInspector.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <numeric>
#include <span>

#include "../../models/EditUtilities.h"
#include "../../models/PsgClip.h"
#include "../../models/PsgFrameRetimer.h"
#include "../../models/PsgRowEvidence.h"
#include "../../models/PsgTempoDetector.h"
#include "../common/LookAndFeel.h"
#include "../common/Transport.h"
#include "../common/Utilities.h"

namespace MoTool {

namespace {

using Theme = Colors::Theme;
using Analysis = PsgTempo::Analysis;

constexpr int mapWindow = 256;                // frames one column of the grid map folds
constexpr int mapHop = 128;                   // frames from one column to the next
constexpr int minPeriod = 2, maxPeriod = 40;  // the grid map's rows: every row grid, and every beat in the BPM range
constexpr float minMapScale = 0.1f;           // the least share the grid map's brightest colour stands for
constexpr int phaseMapBeats = 4;              // beats one column of the phase map folds
constexpr int minMapEvents = 6;  // frames with row evidence a column needs: fewer lie on any grid by chance
constexpr float stripFrameWidth = 3.0f;
constexpr int grid = 8;
constexpr int gutter = grid * 4;  // left of every plot, for its labels, so the plots line up
constexpr int rowHeight = 20;
constexpr int textHeight = 16;   // of a line of the small font
constexpr int axisHeight = 18;   // under a plot, for its axis labels
constexpr int titleHeight = 18;  // over a panel, for its title
constexpr int nameWidth = 48;    // of the name before a fact of the detection's answer

juce::Font smallFont() {
    return juce::Font(juce::FontOptions(13.0f));
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

/** A share of 0..1 in per cent; one under what chance gives reads as 0%. */
juce::String percentText(double share) {
    return juce::String(juce::jmax(0, juce::roundToInt(share * 100.0))) + "%";
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
    Panel(Inspection& inspection, juce::String titleToShow) : in(inspection), title(std::move(titleToShow)) {}

protected:
    /** The plot: right of the gutter and under the title. */
    juce::Rectangle<float> plotArea(int top = 0, int bottom = 0) const {
        return getLocalBounds()
            .withTrimmedLeft(gutter)
            .withTrimmedTop(titleHeight + top)
            .withTrimmedBottom(bottom)
            .toFloat();
    }

    /** What the panel shows, over its plot, in a line. */
    void paintTitle(juce::Graphics& g) const {
        g.setColour(Theme::textSecondary);
        g.setFont(smallFont());
        g.drawText(title, gutter, 0, getWidth() - gutter, textHeight, juce::Justification::centredLeft);
    }

    void label(juce::Graphics& g,
               const juce::String& text,
               float centreY,
               juce::Colour colour = Theme::textSecondary) const {
        g.setColour(colour);
        g.setFont(smallFont());
        g.drawText(text,
                   0,
                   juce::roundToInt(centreY) - textHeight / 2,
                   gutter - 6,
                   textHeight,
                   juce::Justification::centredRight);
    }

    Inspection& in;
    juce::String title;
};

//==============================================================================
/** A map of the tune over time: a column per stretch of it, a row per whatever the map tells
    apart, each cell brighter the more it holds. The colours run up to the map's own brightest
    cell, named under it, so faint structure shows too. A click moves the playhead, and the
    frames the evidence strip shows are marked under the map. */
class TimeMap : public Panel
{
public:
    using Panel::Panel;

    void paint(juce::Graphics& g) override {
        paintTitle(g);
        const auto plot = plotArea(0, axisHeight);
        if (cells.empty() || numRows() == 0)
            return;
        const float cw = columnWidth(), ch = plot.getHeight() / (float) numRows();
        for (size_t c = 0; c < cells.size(); ++c)
            for (size_t r = 0; r < cells[c].size(); ++r) {
                g.setColour(heat(cells[c][r] / scale));
                g.fillRect(plot.getX() + (float) c * cw,
                           plot.getY() + (float) r * ch,
                           juce::jmax(1.0f, cw - 1.0f),
                           juce::jmax(1.0f, ch - 1.0f));
            }

        auto rowY = [&](int row) { return plot.getY() + (float) row * ch; };
        const int marked = markedRow();
        for (int row = 0; row < numRows(); ++row)
            if (row == marked)
                label(g, rowName(row), rowY(row) + ch / 2, Theme::textPrimary);
            else if (isNamed(row) && std::abs(row - marked) > 1)
                label(g, rowName(row), rowY(row) + ch / 2);
        if (marked >= 0) {
            g.setColour(Theme::textPrimary);
            g.drawRect(juce::Rectangle<float>(plot.getX(), rowY(marked), plot.getWidth(), ch).expanded(1.0f), 1.0f);
        }
        paintMarks(g, plot, ch);

        g.setColour(Theme::textSecondary);
        g.setFont(smallFont());
        const int scaleWidth = 110;  // the right end of the axis names the scale
        const auto step = (size_t) juce::jmax(1, (int) std::ceil(60.0f / cw));
        for (size_t c = 0; c < cells.size(); c += step)
            if (const int x = juce::roundToInt(plot.getX() + (float) c * cw);
                x + 40 < (int) plot.getRight() - scaleWidth)
                g.drawText(timeText((double) ((int) c * framesPerColumn) / in.fps),
                           x,
                           (int) plot.getBottom() + 2,
                           60,
                           textHeight,
                           juce::Justification::centredLeft);
        g.drawText("Brightest: " + percentText(scale),
                   (int) plot.getRight() - scaleWidth,
                   (int) plot.getBottom() + 2,
                   scaleWidth,
                   textHeight,
                   juce::Justification::centredRight);

        // the frames the strip shows, and the playhead
        g.setColour(Theme::textSecondary);
        g.fillRect(xOf(in.viewFrame),
                   plot.getBottom(),
                   juce::jmax(2.0f, xOf(in.viewFrame + stripFrames) - xOf(in.viewFrame)),
                   2.0f);
        if (in.playFrame >= 0 && in.playFrame < in.numFrames()) {
            g.setColour(Theme::success);
            g.fillRect(xOf(in.playFrame), plot.getY(), 1.0f, plot.getHeight());
        }
    }

    void mouseDown(const juce::MouseEvent& e) override { mouseDrag(e); }
    void mouseDrag(const juce::MouseEvent& e) override { in.seek(frameAt(e.position.x)); }

    void mouseMove(const juce::MouseEvent& e) override {
        const auto plot = plotArea(0, axisHeight);
        const int column = frameAt(e.position.x) / framesPerColumn;
        const int row = (int) ((e.position.y - plot.getY()) / plot.getHeight() * (float) numRows());
        if (plot.contains(e.position) && column < (int) cells.size() && row < numRows())
            in.say(describe(column, row));
    }

    int stripFrames = 0;  // how many frames the evidence strip shows, for its mark under the map

protected:
    virtual int numRows() const = 0;
    virtual juce::String rowName(int row) const = 0;
    virtual bool isNamed(int row) const = 0;  // whether the row's name is always drawn
    virtual int markedRow() const = 0;        // the beat cursor's row, or -1
    virtual juce::String describe(int column, int row) const = 0;
    virtual void paintMarks(juce::Graphics&, juce::Rectangle<float>, float) {}

    /** Whether a stretch holds enough row evidence to say anything: a few frames lie on any grid by chance. */
    static bool isTelling(std::span<const double> stretch) {
        return std::count_if(stretch.begin(), stretch.end(), [](double e) { return e > 0.0; }) >= minMapEvents;
    }

    std::vector<std::vector<float>> cells;  // [column][row], each a share of 0..1
    float scale = minMapScale;              // the share the brightest colour stands for
    int framesPerColumn = mapHop;           // from one column's start to the next

private:
    float columnWidth() const { return plotArea().getWidth() / (float) std::max<size_t>(1, cells.size()); }
    float xOf(int frame) const { return plotArea().getX() + (float) frame / (float) framesPerColumn * columnWidth(); }
    int frameAt(float x) const {
        return juce::jlimit(0,
                            juce::jmax(0, in.numFrames() - 1),
                            (int) ((x - plotArea().getX()) / columnWidth() * (float) framesPerColumn));
    }
};

//==============================================================================
/** Where a row grid holds: a row per grid period, brighter the more of the stretch's row evidence
    lies on one phase of that period. A tune at one speed is a bright line across, with its
    multiples under it; a slip, a change of speed or a groove breaks it. */
class GridMap : public TimeMap
{
public:
    explicit GridMap(Inspection& inspection)
        : TimeMap(inspection, "Grid map. Rows: grid length in frames. Bright: much row evidence is on this grid") {
        const std::span<const double> evidence(in.analysis.evidence);
        for (size_t start = 0; start < evidence.size(); start += mapHop) {
            const auto stretch = evidence.subspan(start, std::min<size_t>(mapWindow, evidence.size() - start));
            auto& column = cells.emplace_back();
            for (int period = minPeriod; period <= maxPeriod; ++period)
                column.push_back(isTelling(stretch) ? (float) PsgTempo::gridShare(stretch, period) : 0.0f);
            scale = std::max(scale, *std::max_element(column.begin(), column.end()));
            if (start + mapWindow >= evidence.size())
                break;
        }
    }

protected:
    int numRows() const override { return maxPeriod - minPeriod + 1; }
    juce::String rowName(int row) const override { return juce::String(minPeriod + row); }
    bool isNamed(int row) const override { return (minPeriod + row) % 4 == 0; }
    int markedRow() const override { return in.lag <= maxPeriod ? in.lag - minPeriod : -1; }

    juce::String describe(int column, int row) const override {
        return "At " + timeText(column * framesPerColumn / in.fps) + ", a grid of " + juce::String(minPeriod + row)
               + " frames has " + percentText(cells[(size_t) column][(size_t) row])
               + " of the row evidence. A row grid must have " + percentText(PsgTempo::minGridShare)
               + " in the full tune. Click to move the playhead.";
    }
};

//==============================================================================
/** Where the rows fall inside the beat: the tune folded at the beat cursor, a row per frame of the
    beat, brighter the more of the stretch's row evidence starts on that frame. The rows of the
    beat are bright lines across, and the gaps between them are the groove: lines at 0, 8, 16 and
    24 of 31 frames are 8-8-8-7. A line that steps down is a row a frame longer than the beat
    allows; lines that slope are a beat the cursor has a little too short or long. The rows the
    detection read are ticked at the left edge. */
class PhaseMap : public TimeMap
{
public:
    explicit PhaseMap(Inspection& inspection) : TimeMap(inspection, {}) { refresh(); }

    /** Folds the tune at the beat cursor again. */
    void refresh() {
        const std::span<const double> evidence(in.analysis.evidence);
        const auto beat = (size_t) in.lag;
        framesPerColumn = phaseMapBeats * in.lag;
        title = "Phase map. Rows: frame in the beat of " + juce::String(in.lag)
                + " frames. Bright: much row evidence starts on this frame";
        cells.clear();
        scale = minMapScale;
        for (size_t start = 0; start < evidence.size(); start += (size_t) framesPerColumn) {
            const auto stretch = evidence.subspan(start, std::min((size_t) framesPerColumn, evidence.size() - start));
            auto& column = cells.emplace_back(beat, 0.0f);
            const double all = std::accumulate(stretch.begin(), stretch.end(), 0.0);
            if (!isTelling(stretch) || all <= 0.0)
                continue;
            for (size_t i = 0; i < stretch.size(); ++i)
                column[(start + i) % beat] += (float) (stretch[i] / all);
            scale = std::max(scale, *std::max_element(column.begin(), column.end()));
        }
        repaint();
    }

protected:
    int numRows() const override { return in.lag; }
    juce::String rowName(int row) const override { return juce::String(row); }
    bool isNamed(int row) const override { return row % (in.lag > 48 ? 16 : 4) == 0; }
    int markedRow() const override { return -1; }

    juce::String describe(int column, int row) const override {
        return "At " + timeText(column * framesPerColumn / in.fps) + ", frame " + juce::String(row) + " of the beat of "
               + juce::String(in.lag) + " frames has " + percentText(cells[(size_t) column][(size_t) row])
               + " of the row evidence. A bright line is a row of the beat. A line that moves down is a row that is 1 "
                 "frame longer. Click to move the playhead.";
    }

    /** The rows the detection read, when the cursor is on its beat: a tick at the frame each starts on. */
    void paintMarks(juce::Graphics& g, juce::Rectangle<float> plot, float rowHeightPx) override {
        const auto& result = in.analysis.result;
        if (!result || result->framesPerBeat != in.lag)
            return;
        g.setColour(Theme::textPrimary);
        int frame = result->gridPhase;
        for (int row = 0; row < result->rowsPerBeat; ++row) {
            g.fillRect(
                plot.getX() - 5.0f, plot.getY() + ((float) (frame % in.lag) + 0.5f) * rowHeightPx - 0.5f, 4.0f, 1.0f);
            frame += result->rowFrames[(size_t) row % result->rowFrames.size()];
        }
    }
};

//==============================================================================
/** The row evidence frame by frame, around the playhead: what each channel and the chip weigh for
    a row start, and their sum where it counts. Above it the rows the detection settled on, a
    longer tick where a beat starts; below it the beats of the cursor. */
class EvidenceStrip : public Panel
{
public:
    explicit EvidenceStrip(Inspection& inspection)
        : Panel(inspection,
                "Row evidence for each frame. A, B, C: channels. E+N: envelope and noise. Sum: all, where a row start "
                "is probable") {}

    int visibleFrames() const { return juce::jmax(1, (int) (plotArea().getWidth() / stripFrameWidth)); }

    void paint(juce::Graphics& g) override {
        paintTitle(g);
        const auto plot = plotArea(10, 10);
        const int first = in.viewFrame, count = visibleFrames();
        const float rh = plot.getHeight() / 5.0f;
        auto xOf = [&](int frame) { return plot.getX() + (float) (frame - first) * stripFrameWidth; };

        static const char* const names[] = {"A", "B", "C", "E+N", "Sum"};
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
            g.setColour(Theme::success);
            g.fillRect(xOf(in.playFrame), (float) titleHeight, 1.0f, (float) (getHeight() - titleHeight));
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        in.seek(juce::jlimit(0,
                             juce::jmax(0, in.numFrames() - 1),
                             in.viewFrame + (int) ((e.position.x - plotArea().getX()) / stripFrameWidth)));
    }

    void mouseMove(const juce::MouseEvent&) override {
        in.say("Top marks: the detected rows. A long mark is the start of a beat. Bottom marks: the beats of the "
               "cursor. Click to move the playhead.");
    }
};

//==============================================================================
/** How alike the row evidence is to itself by lag: its autocorrelation, with the lags the BPM range
    allows a beat shaded and the least a pulse's beat must repeat drawn across. The beat cursor is
    set here; its doublings are marked, since a beat is a pulse times a power of two. */
class RepeatPlot : public Panel
{
public:
    explicit RepeatPlot(Inspection& inspection)
        : Panel(inspection, "Repeat score for each lag in frames: how much the row evidence repeats after the lag") {}

    void paint(juce::Graphics& g) override {
        paintTitle(g);
        const auto plot = plotArea(4, axisHeight);
        const auto& repeat = in.analysis.repeat;
        if (repeat.size() < 3) {
            g.setColour(Theme::textSecondary);
            g.setFont(smallFont());
            g.drawText("The tune is too short to measure repeats", plot, juce::Justification::centred);
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
                       textHeight,
                       juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& e) override { mouseDrag(e); }

    /** The cursor goes to the lag under the mouse, or to a peak within two frames of it. */
    void mouseDrag(const juce::MouseEvent& e) override {
        const auto plot = plotArea(4, axisHeight);
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
        in.say("Repeat score for each lag in frames. A high score shows that the row evidence repeats after this lag. "
               "Band: the beat lengths that the BPM range permits. Dashed line: the minimum score for a beat without a "
               "row grid. Click or drag to set the cursor.");
    }
};

//==============================================================================
/** The share of the whole tune's row evidence on each uniform row grid, against the share a grid needs. */
class GridBars : public Panel
{
public:
    explicit GridBars(Inspection& inspection) : Panel(inspection, "Row evidence on each grid length, full tune") {
        for (const auto& grid : in.analysis.grids)
            if (grid.rows.size() == 1)
                uniform.push_back(grid);
    }

    void paint(juce::Graphics& g) override {
        paintTitle(g);
        const auto plot = plotArea(4, axisHeight);
        if (uniform.empty()) {
            g.setColour(Theme::textSecondary);
            g.setFont(smallFont());
            g.drawText("The tune has too little row evidence", plot, juce::Justification::centred);
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
                       juce::Rectangle<float>(x - 4.0f, plot.getBottom() + 2.0f, bw + 8.0f, (float) textHeight),
                       juce::Justification::centred);
        }
        g.setColour(Theme::border);
        const float dashes[] = {3.0f, 3.0f};
        g.drawDashedLine(
            {plot.getX(), yOf(PsgTempo::minGridShare), plot.getRight(), yOf(PsgTempo::minGridShare)}, dashes, 2);
        label(g, percentText(PsgTempo::minGridShare), yOf(PsgTempo::minGridShare));
    }

    void mouseDown(const juce::MouseEvent& e) override {
        if (const auto* grid = gridAt(e.position.x))
            in.setLag(grid->rows[0]);
    }

    void mouseMove(const juce::MouseEvent& e) override {
        if (const auto* grid = gridAt(e.position.x))
            in.say("A grid of " + juce::String(grid->rows[0]) + " frames has " + percentText(grid->share)
                   + " of the row evidence in the full tune. The minimum is " + percentText(PsgTempo::minGridShare)
                   + ". Click to set the cursor.");
    }

private:
    const Analysis::Grid* gridAt(float x) const {
        const auto plot = plotArea(4, axisHeight);
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
    explicit BeatTable(Inspection& inspection) : Panel(inspection, "Beats that the detection compared") {}

    int idealHeight() const { return titleHeight + rowHeight * ((int) in.analysis.beats.size() + 2); }

    void paint(juce::Graphics& g) override {
        paintTitle(g);
        static const char* const headers[] = {"Beat", "Frames", "BPM", "Drums and bass", "Accents", "Repeat"};
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
            const juce::String texts[] = {isCursor ? juce::String("Cursor")
                                                   : juce::String(about ? "Approximately " : "")
                                                         + juce::String(beat.rowsPerBeat) + (pulses ? " pulse" : " row")
                                                         + (beat.rowsPerBeat == 1 ? "" : "s")
                                                         + (chosen ? " (detected)" : ""),
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
        const int row = ((int) e.position.y - titleHeight) / rowHeight - 1;
        if (row >= 0 && row < (int) in.analysis.beats.size())
            in.setLag(in.analysis.beats[(size_t) row].frames);
    }

    void mouseMove(const juce::MouseEvent&) override {
        in.say("Drums and bass: the part of the rows that sound the same 2, 4 and 8 beats later. Accents: how well "
               "loud rows repeat after 1 beat. Repeat: the repeat score for 1, 2 and 4 beats. Click a line to set the "
               "cursor.");
    }

private:
    /** The beat's name takes the widest column: it can read "Approximately 4 pulses (detected)". */
    juce::Rectangle<float> cell(int row, int column) const {
        static constexpr float starts[] = {0.0f, 0.32f, 0.44f, 0.56f, 0.74f, 0.87f, 1.0f};
        const float width = (float) (getWidth() - gutter);
        return {(float) gutter + starts[column] * width,
                (float) (titleHeight + row * rowHeight),
                (starts[column + 1] - starts[column]) * width,
                (float) rowHeight};
    }
};

//==============================================================================
/** One thing the detection answered, and what it found about it: a sentence or two to a line. */
struct Fact
{
    juce::String name;
    juce::StringArray lines;
};

/** How the detection came to its answer: the tempo, the rows and the meter, in short sentences. */
std::vector<Fact> describe(const Inspection& in) {
    using RowsFrom = Analysis::RowsFrom;
    const auto& a = in.analysis;
    const auto minimum = " The minimum is " + percentText(PsgTempo::minGridShare) + ".";
    std::vector<Fact> facts;

    if (const auto& r = a.result) {
        juce::String tempo = juce::String(r->bpm, 2) + " BPM. A beat is " + juce::String(r->framesPerBeat) + " frames";
        if (!r->rowsArePulse || r->rowsPerBeat > 1)
            tempo << ": " << juce::String(r->rowsPerBeat)
                  << (r->rowsArePulse && a.grooveAgreement <= 0.0 ? " pulse" : " row")
                  << (r->rowsPerBeat == 1 ? "" : "s") << " of " << rowsText(r->rowFrames) << " frames";
        facts.push_back({"Tempo", {tempo + "."}});
    } else {
        facts.push_back({"Tempo", {"Not detected."}});
    }

    const Analysis::Grid* best = nullptr;
    juce::StringArray patterns;
    for (const auto& grid : a.grids) {
        if (grid.rows.size() == 1 && (best == nullptr || grid.share > best->share))
            best = &grid;
        if (grid.rows.size() > 1)
            patterns.add(juce::String(grid.rows.size() == 2 ? "Best swing: " : "Best groove: ") + rowsText(grid.rows)
                         + " frames, " + percentText(grid.share) + ".");
    }
    Fact rows{"Rows", {}};
    if (a.rowsFrom == RowsFrom::evidence)
        rows.lines.add("Detected. A grid of " + rowsText(a.result->rowFrames) + " frames has "
                       + percentText(a.result->gridShare) + " of the row evidence." + minimum);
    else if (best == nullptr)
        rows.lines.add(juce::String(a.rowsFrom == RowsFrom::noteStarts ? "Detected from note starts." : "Not detected.")
                       + " The tune has too little row evidence.");
    else
        rows.lines.add(juce::String(a.rowsFrom == RowsFrom::noteStarts ? "Detected from note starts." : "Not detected.")
                       + " The best grid, " + juce::String(best->rows[0]) + " frames, has only "
                       + percentText(best->share) + " of the row evidence." + minimum);

    if (a.rowsFrom == RowsFrom::noteStarts) {
        rows.lines.add("A grid of the note starts has " + percentText(a.result->gridShare)
                       + ". The detector uses this grid.");
    } else if (a.rowsFrom == RowsFrom::pulse) {
        const bool whole = a.result->framesPerBeat % a.pulse == 0;  // the beat is a whole number of pulses
        const auto repeats = "The evidence repeats after each " + juce::String(a.pulse) + " frames";
        if (!whole)
            rows.lines.add(repeats
                           + ", but 2 and 4 pulses are not at the repeat peaks. The beat is at the nearest peak: "
                           + juce::String(a.result->framesPerBeat) + " frames.");
        else
            rows.lines.add(repeats + (a.grooveAgreement > 0.0 ? "." : ". This pulse replaces the rows."));
        if (a.grooveAgreement > 0.0)
            rows.lines.add("The rows in the beat are " + rowsText(a.result->rowFrames) + " frames in "
                           + percentText(a.grooveAgreement) + " of the tune. The minimum is "
                           + percentText(PsgTempo::minGrooveAgreement) + ".");
    } else if (a.rowsFrom == RowsFrom::nothing && a.pulse > 0) {
        rows.lines.add("The evidence repeats after each " + juce::String(a.pulse)
                       + " frames, but no beat of this pulse has the minimum repeat score of "
                       + juce::String(PsgTempo::minBeatRepeat, 2) + ".");
    } else if (a.rowsFrom == RowsFrom::nothing && best != nullptr) {
        rows.lines.add(a.repeat.empty() ? "The tune is too short to measure repeats."
                                        : "No interval has a clear repeat.");
    }
    if (!patterns.isEmpty())
        rows.lines.add(patterns.joinIntoString(" "));
    facts.push_back(rows);

    if (const auto& r = a.result) {
        const bool triple = a.meterMargin > 0.0;
        const juce::String better = triple ? "Bars of 3 beats repeat better than bars of 4 beats"
                                           : "Bars of 4 beats repeat better than bars of 3 beats";
        const auto difference = juce::String(std::abs(a.meterMargin), 2);
        const auto least = " The minimum is " + juce::String(PsgTempo::minMeterMargin, 2) + ".";
        facts.push_back(
            {"Meter",
             {r->meterClear ? juce::String(triple ? "3/4. " : "4/4. ") + better + ". The difference is " + difference
                                  + "." + least
                            : "Not clear. " + better + ", but the difference is only " + difference + "." + least}});
    }
    return facts;
}

/** The detection's answer: each fact's name, and its lines beside it. */
class FactsView : public juce::Component
{
public:
    explicit FactsView(std::vector<Fact> factsToShow) : facts(std::move(factsToShow)) {}

    /** The height the facts take when the view is this wide. */
    int heightFor(int width) const {
        float height = 0.0f;
        for (const auto& fact : facts) {
            for (const auto& line : fact.lines)
                height += layoutOf(line, Theme::textPrimary, width - nameWidth).getHeight();
            height += factGap;
        }
        return (int) std::ceil(height - factGap);
    }

    void paint(juce::Graphics& g) override {
        float y = 0.0f;
        for (const auto& fact : facts) {
            // the name is laid out as its lines are, so both sit on one baseline
            const auto name = layoutOf(fact.name, Theme::textSecondary, nameWidth);
            name.draw(g, {0.0f, y, (float) nameWidth, name.getHeight()});
            for (const auto& line : fact.lines) {
                const auto layout = layoutOf(line, Theme::textPrimary, getWidth() - nameWidth);
                layout.draw(g, {(float) nameWidth, y, (float) (getWidth() - nameWidth), layout.getHeight()});
                y += layout.getHeight();
            }
            y += factGap;
        }
    }

private:
    static constexpr float factGap = 3.0f;  // between one fact's lines and the next fact

    static juce::TextLayout layoutOf(const juce::String& line, juce::Colour colour, int width) {
        juce::AttributedString text;
        text.append(line, smallFont(), colour);
        juce::TextLayout layout;
        layout.createLayout(text, (float) juce::jmax(1, width));
        return layout;
    }

    std::vector<Fact> facts;
};

//==============================================================================
class TempoInspector : public juce::Component, private juce::Timer
{
public:
    explicit TempoInspector(PsgClip& clip)
        : clip_(&clip), clickWasOn_(clip.edit.clickTrackEnabled.get()), map_(in), phases_(in), strip_(in), repeat_(in),
          bars_(in), table_(in), play_(clip.edit) {
        setWantsKeyboardFocus(true);
        for (auto* panel : std::initializer_list<juce::Component*>{&map_, &phases_, &strip_, &repeat_, &bars_, &table_})
            addAndMakeVisible(panel);
        for (auto* button :
             std::initializer_list<juce::Button*>{&play_, &metronome_, &phaseMap_, &apply_ /*, &copy_*/}) {
            button->setWantsKeyboardFocus(false);
            addAndMakeVisible(button);
        }
        for (auto* text : {&cursor_, &status_}) {
            text->setFont(smallFont());
            text->setBorderSize({});  // its text starts at the plots' left edge
            text->setMinimumHorizontalScale(1.0f);
            addAndMakeVisible(text);
        }
        addAndMakeVisible(facts_);
        status_.setColour(juce::Label::textColourId, Theme::textSecondary);
        status_.setJustificationType(juce::Justification::topLeft);

        metronome_.setToggleState(clickWasOn_, juce::dontSendNotification);
        metronome_.onClick = [this] {
            if (auto* c = clip_.get())
                c->edit.clickTrackEnabled = metronome_.getToggleState();
        };
        // The phase map is the one to look at when no row grid holds: the grid map is dark then
        using RowsFrom = Analysis::RowsFrom;
        phaseMap_.setToggleState(in.analysis.rowsFrom != RowsFrom::evidence
                                     && in.analysis.rowsFrom != RowsFrom::noteStarts,
                                 juce::dontSendNotification);
        phaseMap_.onClick = [this] { showMap(); };
        apply_.onClick = [this] { applyCursorBeat(); };
        // Copy label: the clip's name and the cursor's BPM as a line for scripts/psg_tempo_labels.csv.
        // Off: nobody labels tunes that way yet.
        // copy_.onClick = [this] {
        //     if (auto* c = clip_.get())
        //         juce::SystemClipboard::copyTextToClipboard(c->getName().quoted() + ","
        //                                                    + juce::String(in.bpmOf(in.lag), 2));
        // };

        in.lagChanged = [this] {
            cursor_.setText("Cursor: " + juce::String(in.lag) + " frames, " + juce::String(in.bpmOf(in.lag), 2)
                                + " BPM",
                            juce::dontSendNotification);
            phases_.refresh();
            repaint();
        };
        in.say = [this](const juce::String& text) { status_.setText(text, juce::dontSendNotification); };
        in.seek = [this](int frame) {
            if (auto* c = clip_.get())
                c->edit.getTransport().setPosition(timeOf(*c, frame));
        };
        const auto& result = in.analysis.result;
        in.setLag(result ? result->framesPerBeat : in.analysis.pulse > 0 ? in.analysis.pulse : 24);
        setSize(880, 860);
        showMap();
        startTimerHz(30);
    }

    ~TempoInspector() override {
        if (auto* c = clip_.get())
            c->edit.clickTrackEnabled = clickWasOn_;  // the metronome is the inspector's: a project doesn't keep it
    }

    void paint(juce::Graphics& g) override { g.fillAll(Theme::background); }

    void resized() override {
        auto area = getLocalBounds()
                        .withTrimmedLeft(grid * 2)
                        .withTrimmedTop(grid * 2)
                        .withTrimmedRight(grid * 2 + gutter)
                        .withTrimmedBottom(grid * 2);
        // Buttons and text start where the plots do, right of the gutter that holds the plots' labels
        auto buttons = area.removeFromTop(grid * 4).withTrimmedLeft(gutter);
        play_.setBounds(buttons.removeFromLeft(buttons.getHeight()));  // a square, as in the transport bar
        buttons.removeFromLeft(grid * 2);

        metronome_.setBounds(buttons.removeFromLeft(110));
        phaseMap_.setBounds(buttons.removeFromLeft(110));
        // copy_.setBounds(buttons.removeFromRight(100));
        // buttons.removeFromRight(grid * 2);
        apply_.setBounds(buttons.removeFromRight(180));
        cursor_.setBounds(buttons.removeFromRight(180));
        area.removeFromTop(grid * 2);

        facts_.setBounds(area.removeFromTop(facts_.heightFor(area.getWidth() - gutter)).withTrimmedLeft(gutter));
        area.removeFromTop(grid * 2);

        status_.setBounds(area.removeFromBottom(grid * 4).withTrimmedLeft(gutter));
        area.removeFromBottom(grid * 2);  // the status line stands apart from the table

        table_.setBounds(area.removeFromBottom(table_.idealHeight()));
        area.removeFromBottom(grid * 2);

        auto plots = area.removeFromBottom(150 + titleHeight);
        bars_.setBounds(plots.removeFromRight(300));
        repeat_.setBounds(plots);
        area.removeFromBottom(grid * 2);

        strip_.setBounds(area.removeFromBottom(100 + titleHeight));
        area.removeFromBottom(grid * 2);

        map_.setBounds(area);
        phases_.setBounds(area);

        map_.stripFrames = phases_.stripFrames = strip_.visibleFrames();
    }

    bool keyPressed(const juce::KeyPress& key) override {
        if (key != juce::KeyPress::spaceKey)
            return false;
        play_.triggerClick();
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

    tracktion::TimePosition timeOf(const PsgClip& clip, int frame) const {
        const auto position = clip.getPosition();
        return position.getStart() + tracktion::TimeDuration::fromSeconds((frame + clip.getLeadInFrames()) / in.fps)
               - position.getOffset();
    }

    int frameAt(const PsgClip& clip, tracktion::TimePosition time) const {
        const auto position = clip.getPosition();
        return (int) std::floor((time - position.getStart() + position.getOffset()).inSeconds() * in.fps)
               - clip.getLeadInFrames();
    }

    /** The grid map and the phase map share their place: the switch shows one. */
    void showMap() {
        phases_.setVisible(phaseMap_.getToggleState());
        map_.setVisible(!phaseMap_.getToggleState());
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
        // The playhead stands where the tune is heard, as the timeline's does: the transport runs ahead of
        // the sound by the output device's latency.
        // ponytail: no wait after a jump, which the timeline's playhead has: for one latency it shows the
        // frames just before the place jumped to.
        auto& transport = c->edit.getTransport();
        const auto heard = transport.isPlaying() ? Helpers::getHeardPosition(c->edit) : transport.getPosition();
        const int frame = frameAt(*c, heard);
        if (frame == in.playFrame)
            return;
        in.playFrame = frame;
        // the strip keeps the playhead a third of the way in
        in.viewFrame = juce::jmax(0, frame - strip_.visibleFrames() / 3);
        map_.repaint();
        phases_.repaint();
        strip_.repaint();
    }

    tracktion::SafeSelectable<PsgClip> clip_;
    const bool clickWasOn_;
    Inspection in{inspect(*clip_.get())};
    GridMap map_;
    PhaseMap phases_;
    EvidenceStrip strip_;
    RepeatPlot repeat_;
    GridBars bars_;
    BeatTable table_;
    PlayButton play_;
    juce::TextButton apply_{"Set as project tempo"};  // , copy_{"Copy label"};
    juce::ToggleButton metronome_{"Metronome"}, phaseMap_{"Phase map"};
    FactsView facts_{describe(in)};
    juce::Label cursor_, status_;

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
