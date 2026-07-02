#pragma once

#include <JuceHeader.h>

#include "Timecode.h"

namespace MoTool {

//==============================================================================
/** An ordered ladder of timeline grid/snap subdivision levels for one timecode
    display mode, modeled on te::TimecodeSnapType: levels run from finest to
    coarsest, and each level can round times onto its grid, report its
    approximate on-screen interval and produce a ruler label.

    Unlike Tracktion's beat-fraction levels, the frame-bearing modes subdivide
    in integer frame space: frames restart at each beat (barsBeatsFrames) or at
    each second (framesOnly), and an optional subdivision pattern (e.g. 6-7-6-7
    for 26 frames per beat) inserts a swing-capable level between the frames
    and the beat. Frame levels restart at each subdivision boundary, so every
    level's ticks are a superset of the ticks of all coarser levels' boundaries.
*/
class SnapLadder {
public:
    enum class LevelKind { teSnap, frames, subdivision, beat, bar };

    /** framesPerBeat is the nominal frame count of one beat, used to prune the
        frame-multiple levels and to lay out the subdivision pattern. Rounding
        always recomputes the exact per-beat frame count from the tempo. */
    SnapLadder(TimecodeDisplayMode mode, double fps, int framesPerBeat,
               std::vector<int> subdivisionPattern = {});

    int getNumLevels() const noexcept  { return (int) levels_.size(); }
    LevelKind getKind(int level) const noexcept;

    /** Index of the beat level, or -1 for modes without one. */
    int getBeatLevel() const noexcept  { return beatLevel_; }

    te::TimePosition roundDown(int level, te::TimePosition, const te::TempoSequence&) const;
    te::TimePosition roundNearest(int level, te::TimePosition, const te::TempoSequence&) const;
    te::TimePosition roundUp(int level, te::TimePosition, const te::TempoSequence&) const;

    /** The first tick of this level strictly after the given time. */
    te::TimePosition getNextTick(int level, te::TimePosition, const te::TempoSequence&) const;

    bool isOnGrid(int level, te::TimePosition, const te::TempoSequence&) const;

    /** Coarsest level whose grid contains the given time. */
    int getMaxLevelAt(te::TimePosition, const te::TempoSequence&) const;

    te::TimeDuration getApproxInterval(int level, const te::TempoSetting&, bool triplets) const;

    /** First level whose ticks are at least minPixels apart on screen
        (Tracktion's heuristic), or the coarsest level if none is. */
    int getBestLevel(te::TimeDuration onScreenTimePerPixel, double minPixels,
                     const te::TempoSetting&, bool triplets) const;

    /** The next structurally significant level above the given one: the beat
        for sub-beat levels, the bar for the beat, then increasingly coarse bar
        multiples (or simply two levels up for the non-musical modes). */
    int getMilestoneAfter(int level) const;

    /** Ruler label for a tick of this level ("3", "1.2", "1.1.05", "0:00:05"). */
    juce::String getLabel(int level, te::TimePosition, const te::TempoSequence&) const;

private:
    struct Level {
        LevelKind kind;
        int multiple = 1;           // frame multiple, or bar multiple
        te::TimecodeSnapType snap;  // valid for teSnap/beat/bar levels
    };

    /** The container a frame-level time lives in: one beat (barsBeatsFrames)
        or one second (framesOnly), with the queried time as a frame offset. */
    struct FrameContext {
        te::TimePosition start, end;
        int frameCount = 1;
        double frame = 0.0;
    };

    FrameContext frameContextAt(te::TimePosition, const te::TempoSequence&) const;
    std::vector<int> groupBoundariesFor(int frameCount) const;
    double allowedFrameDown(const Level&, double frame, const std::vector<int>& bounds) const;
    double allowedFrameUp(const Level&, double frame, const std::vector<int>& bounds) const;
    bool isAllowedFrame(const Level&, int frame, const std::vector<int>& bounds) const;
    te::TimePosition frameToTime(const FrameContext&, double frame) const;
    te::TimePosition roundFrameKind(const Level&, te::TimePosition, const te::TempoSequence&, int direction) const;

    TimecodeDisplayMode mode_;
    double fps_ = 50.0;
    std::vector<int> pattern_;
    std::vector<Level> levels_;
    int beatLevel_ = -1;
    int barLevel_ = -1;
    int firstTeLevel_ = -1;  // framesOnly: index of the seconds level
};

}  // namespace MoTool
