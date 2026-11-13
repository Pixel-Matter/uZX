#include "SnapLadder.h"

#include <numeric>

using namespace juce;
using namespace std::literals;

namespace MoTool {

namespace {

// Tolerance for treating a fractional frame position as an integer frame.
constexpr double frameTolerance = 1.0e-3;

// Tolerance for the teSnap on-grid check, same as Tracktion's
// getSnapTypeForMaximumSnapLevelOf.
constexpr double timeTolerance = 1.0e-6;

// Round-number frame multiples, the same family the grid always used.
constexpr std::array beatFrameMultiples { 1, 4, 5, 10, 20, 25, 50, 60, 100, 200, 400 };

// Round-number in-second frame multiples. Clock-mode frame ticks restart every
// second, so a step must divide it: rates that 5 doesn't divide (24, 48 fps)
// step in sixths and twelfths instead.
constexpr std::array frameMultiples { 1, 2, 5, 10, 20, 25, 50, 100, 200, 500 };
constexpr std::array filmFrameMultiples { 1, 2, 6, 12 };

// Bar multiples matching te::TimecodeSnapType levels 10..18.
constexpr std::array barMultiples { 1, 2, 4, 8, 16, 64, 128, 256, 1024 };

constexpr int roundDownDir = -1, roundNearestDir = 0, roundUpDir = 1;

/** MM:SS:FF at the project fps; minutes keep counting past the hour. The frame
    field is as wide as the second's last frame, so 200 fps reads MM:SS:FFF. */
String clockLabel(te::TimePosition t, double fps) {
    const auto framesPerSecond = std::max(1, roundToInt(fps));
    const auto frames = roundToInt(std::max(0.0, t.inSeconds()) * fps);
    const auto seconds = frames / framesPerSecond;
    const auto frameDigits = std::max(2, String(framesPerSecond - 1).length());
    return String::formatted("%02d:%02d:", seconds / 60, seconds % 60)
         + String(frames % framesPerSecond).paddedLeft('0', frameDigits);
}

double snapToIntegerFrame(double frame) {
    auto rounded = std::round(frame);
    return std::abs(frame - rounded) < frameTolerance ? rounded : frame;
}

}  // namespace

//==============================================================================
SnapLadder::SnapLadder(TimecodeDisplayMode mode, double fps, int framesPerBeat,
                       std::vector<int> subdivisionPattern)
    : mode_(mode)
    , fps_(std::max(1.0, fps))
    , pattern_(std::move(subdivisionPattern))
{
    if (pattern_.size() < 2 || mode_ != TimecodeDisplayMode::barsBeatsFrames)
        pattern_.clear();

    auto addTeLevels = [this](te::TimecodeType type, int first, int last) {
        for (int i = first; i <= last; ++i)
            levels_.push_back({ LevelKind::teSnap, 1, te::TimecodeSnapType(type, i) });
    };

    // Prune coarse multiples, but always retain single frames for deep zoom.
    // A clock step must also divide the second its ticks restart at.
    auto addFrameLevels = [this](int containerFrames, const auto& multiples) {
        for (auto m : multiples)
            if (m == 1 || (m * 4 <= containerFrames && (! isClock() || containerFrames % m == 0)))
                levels_.push_back({ LevelKind::frames, m, {} });
    };

    switch (mode_) {
        case TimecodeDisplayMode::barsBeatsFrames: {
            auto bounds = groupBoundariesFor(std::max(1, framesPerBeat));
            auto minGroup = std::max(1, framesPerBeat);
            for (size_t i = 1; i < bounds.size(); ++i)
                minGroup = std::min(minGroup, bounds[i] - bounds[i - 1]);

            addFrameLevels(minGroup, beatFrameMultiples);

            if (! pattern_.empty())
                levels_.push_back({ LevelKind::subdivision, 1, {} });

            beatLevel_ = (int) levels_.size();
            levels_.push_back({ LevelKind::beat, 1,
                                te::TimecodeSnapType(te::TimecodeType::barsBeats, 9) });

            barLevel_ = (int) levels_.size();
            for (size_t i = 0; i < barMultiples.size(); ++i)
                levels_.push_back({ LevelKind::bar, barMultiples[i],
                                    te::TimecodeSnapType(te::TimecodeType::barsBeats, 10 + (int) i) });
            break;
        }

        case TimecodeDisplayMode::seconds:
        case TimecodeDisplayMode::framesOnly: {
            const auto framesPerSecond = std::max(1, roundToInt(fps_));
            if (framesPerSecond % 5 == 0)
                addFrameLevels(framesPerSecond, frameMultiples);
            else
                addFrameLevels(framesPerSecond, filmFrameMultiples);
            firstTeLevel_ = (int) levels_.size();
            addTeLevels(te::TimecodeType::millisecs, 4, 12);  // second .. 30 minutes
            break;
        }
    }

    jassert(! levels_.empty());
}

SnapLadder::LevelKind SnapLadder::getKind(int level) const noexcept {
    jassert(isPositiveAndBelow(level, getNumLevels()));
    return levels_[(size_t) level].kind;
}

//==============================================================================
SnapLadder::FrameContext SnapLadder::frameContextAt(te::TimePosition t, const te::TempoSequence& ts) const {
    FrameContext ctx;

    if (isClock()) {
        const auto framesPerSecond = std::max(1, roundToInt(fps_));
        const auto totalFrames = snapToIntegerFrame(std::max(0.0, t.inSeconds()) * fps_);
        const auto second = std::floor(totalFrames / framesPerSecond);
        ctx.start = te::TimePosition::fromSeconds(second);
        ctx.end = te::TimePosition::fromSeconds(second + 1.0);
        ctx.frameCount = framesPerSecond;
        ctx.frame = totalFrames - second * framesPerSecond;
        return ctx;
    }

    // toBeats(toTime(b)) can round-trip a hair below the whole beat; snap it so
    // container boundaries always resolve to the container they start.
    auto beats = ts.toBeats(t).inBeats();
    const auto nearestWhole = std::round(beats);
    if (std::abs(beats - nearestWhole) < 1.0e-6)
        beats = nearestWhole;

    const auto wholeBeat = std::floor(beats);
    ctx.start = ts.toTime(te::BeatPosition::fromBeats(wholeBeat));
    ctx.end = ts.toTime(te::BeatPosition::fromBeats(wholeBeat + 1.0));
    ctx.frameCount = std::max(1, roundToInt((ctx.end - ctx.start).inSeconds() * fps_));
    ctx.frame = snapToIntegerFrame((t - ctx.start).inSeconds() * fps_);
    return ctx;
}

std::vector<int> SnapLadder::groupBoundariesFor(int frameCount) const {
    std::vector<int> bounds { 0 };

    if (! pattern_.empty()) {
        if (std::accumulate(pattern_.begin(), pattern_.end(), 0) == frameCount) {
            int acc = 0;
            for (size_t i = 0; i + 1 < pattern_.size(); ++i) {
                acc += pattern_[i];
                bounds.push_back(acc);
            }
        } else {
            // The pattern doesn't match this beat's frame count (e.g. after a
            // tempo change) — keep the group count and spread frames evenly.
            const auto groups = (int) pattern_.size();
            for (int i = 1; i < groups; ++i)
                bounds.push_back((i * frameCount) / groups);
        }
    }

    bounds.push_back(frameCount);
    return bounds;
}

double SnapLadder::allowedFrameDown(const Level& lvl, double frame, const std::vector<int>& bounds) const {
    if (frame <= 0.0)
        return 0.0;
    if (frame >= (double) bounds.back())
        return (double) bounds.back();

    double groupStart = 0.0;
    for (auto b : bounds) {
        if ((double) b <= frame)
            groupStart = (double) b;
        else
            break;
    }

    if (lvl.kind == LevelKind::subdivision)
        return groupStart;

    return groupStart + lvl.multiple * std::floor((frame - groupStart) / lvl.multiple);
}

double SnapLadder::allowedFrameUp(const Level& lvl, double frame, const std::vector<int>& bounds) const {
    if (frame <= 0.0)
        return 0.0;
    if (frame >= (double) bounds.back())
        return (double) bounds.back();

    double groupStart = 0.0, groupEnd = (double) bounds.back();
    for (size_t i = 0; i + 1 < bounds.size(); ++i) {
        if ((double) bounds[i] <= frame && frame < (double) bounds[i + 1]) {
            groupStart = (double) bounds[i];
            groupEnd = (double) bounds[i + 1];
            break;
        }
    }

    if (lvl.kind == LevelKind::subdivision)
        return frame == groupStart ? groupStart : groupEnd;

    auto candidate = groupStart + lvl.multiple * std::ceil((frame - groupStart) / lvl.multiple);
    return std::min(candidate, groupEnd);
}

bool SnapLadder::isAllowedFrame(const Level& lvl, int frame, const std::vector<int>& bounds) const {
    if (frame <= 0 || frame >= bounds.back())
        return true;  // container boundaries are on every frame grid

    if (lvl.kind == LevelKind::subdivision)
        return std::find(bounds.begin(), bounds.end(), frame) != bounds.end();

    int groupStart = 0;
    for (auto b : bounds) {
        if (b <= frame)
            groupStart = b;
        else
            break;
    }
    return (frame - groupStart) % lvl.multiple == 0;
}

te::TimePosition SnapLadder::frameToTime(const FrameContext& ctx, double frame) const {
    if (frame >= (double) ctx.frameCount)
        return ctx.end;  // exact container boundary, no float drift
    if (frame <= 0.0)
        return ctx.start;
    return ctx.start + te::TimeDuration::fromSeconds(frame / fps_);
}

te::TimePosition SnapLadder::roundFrameKind(const Level& lvl, te::TimePosition t,
                                            const te::TempoSequence& ts, int direction) const {
    const auto ctx = frameContextAt(t, ts);
    const auto bounds = groupBoundariesFor(ctx.frameCount);
    const auto down = allowedFrameDown(lvl, ctx.frame, bounds);
    const auto up = allowedFrameUp(lvl, ctx.frame, bounds);

    if (direction == roundDownDir)
        return frameToTime(ctx, down);
    if (direction == roundUpDir)
        return frameToTime(ctx, up);
    return frameToTime(ctx, ctx.frame - down <= up - ctx.frame ? down : up);
}

//==============================================================================
te::TimePosition SnapLadder::roundDown(int level, te::TimePosition t, const te::TempoSequence& ts) const {
    const auto& lvl = levels_[(size_t) level];
    if (lvl.kind == LevelKind::frames || lvl.kind == LevelKind::subdivision)
        return roundFrameKind(lvl, t, ts, roundDownDir);
    return lvl.snap.roundTimeDown(t, ts);
}

te::TimePosition SnapLadder::roundNearest(int level, te::TimePosition t, const te::TempoSequence& ts) const {
    const auto& lvl = levels_[(size_t) level];
    if (lvl.kind == LevelKind::frames || lvl.kind == LevelKind::subdivision)
        return roundFrameKind(lvl, t, ts, roundNearestDir);
    return lvl.snap.roundTimeNearest(t, ts);
}

te::TimePosition SnapLadder::roundUp(int level, te::TimePosition t, const te::TempoSequence& ts) const {
    const auto& lvl = levels_[(size_t) level];
    if (lvl.kind == LevelKind::frames || lvl.kind == LevelKind::subdivision)
        return roundFrameKind(lvl, t, ts, roundUpDir);
    return lvl.snap.roundTimeUp(t, ts);
}

te::TimePosition SnapLadder::getNextTick(int level, te::TimePosition t, const te::TempoSequence& ts) const {
    const auto& lvl = levels_[(size_t) level];

    if (lvl.kind == LevelKind::frames || lvl.kind == LevelKind::subdivision) {
        auto ctx = frameContextAt(t, ts);

        if (ctx.frame >= (double) ctx.frameCount)  // effectively at the next container's start
            ctx = frameContextAt(ctx.end, ts);

        const auto bounds = groupBoundariesFor(ctx.frameCount);
        const auto queryFrame = ctx.frame == std::floor(ctx.frame) ? ctx.frame + 0.5 : ctx.frame;
        return frameToTime(ctx, allowedFrameUp(lvl, queryFrame, bounds));
    }

    auto next = lvl.snap.roundTimeUp(t + te::TimeDuration::fromSeconds(1.0e-5), ts);

    if (next <= t) {
        jassertfalse;
        if (auto* tempo = ts.getTempo(0))
            next = t + lvl.snap.getApproxIntervalTime(*tempo, false);
    }

    return next;
}

bool SnapLadder::isOnGrid(int level, te::TimePosition t, const te::TempoSequence& ts) const {
    const auto& lvl = levels_[(size_t) level];

    if (lvl.kind == LevelKind::frames || lvl.kind == LevelKind::subdivision) {
        const auto ctx = frameContextAt(t, ts);
        if (ctx.frame != std::floor(ctx.frame))
            return false;
        return isAllowedFrame(lvl, (int) ctx.frame, groupBoundariesFor(ctx.frameCount));
    }

    return std::abs((lvl.snap.roundTimeNearest(t, ts) - t).inSeconds()) < timeTolerance;
}

int SnapLadder::getMaxLevelAt(te::TimePosition t, const te::TempoSequence& ts) const {
    for (int i = getNumLevels(); --i > 0;)
        if (isOnGrid(i, t, ts))
            return i;
    return 0;
}

//==============================================================================
te::TimeDuration SnapLadder::getApproxInterval(int level, const te::TempoSetting& tempo, bool triplets) const {
    const auto& lvl = levels_[(size_t) level];

    switch (lvl.kind) {
        case LevelKind::frames:
            return te::TimeDuration::fromSeconds(lvl.multiple / fps_);
        case LevelKind::subdivision:
            return tempo.getApproxBeatLength() / (double) std::max<size_t>(1, pattern_.size());
        case LevelKind::beat:
        case LevelKind::bar:
        case LevelKind::teSnap:
            return lvl.snap.getApproxIntervalTime(tempo, triplets);
    }

    jassertfalse;
    return {};
}

int SnapLadder::getBestLevel(te::TimeDuration onScreenTimePerPixel, double minPixels,
                             const te::TempoSetting& tempo, bool triplets) const {
    for (int i = 0; i < getNumLevels(); ++i) {
        auto interval = getApproxInterval(i, tempo, triplets);
        if (interval.inSeconds() / onScreenTimePerPixel.inSeconds() >= minPixels)
            return i;
    }
    return getNumLevels() - 1;
}

int SnapLadder::getMilestoneAfter(int level) const {
    const auto clampLevel = [this](int l) { return jlimit(0, getNumLevels() - 1, l); };

    switch (getKind(level)) {
        case LevelKind::frames:
            if (isClock())
                return firstTeLevel_;
            if (beatLevel_ > 0 && levels_[(size_t) beatLevel_ - 1].kind == LevelKind::subdivision)
                return beatLevel_ - 1;
            return beatLevel_;
        case LevelKind::subdivision:
            return beatLevel_;
        case LevelKind::beat:
            return barLevel_;
        case LevelKind::bar:
            return clampLevel(level + 2);
        case LevelKind::teSnap:
            return clampLevel(level + 2);
    }

    jassertfalse;
    return clampLevel(level);
}

//==============================================================================
String SnapLadder::getLabel(int level, te::TimePosition t, const te::TempoSequence& ts) const {
    if (mode_ == TimecodeDisplayMode::seconds)
        return clockLabel(t, fps_);
    if (mode_ == TimecodeDisplayMode::framesOnly)
        return String(roundToInt(t.inSeconds() * fps_));

    const auto& lvl = levels_[(size_t) level];

    if (lvl.kind == LevelKind::frames || lvl.kind == LevelKind::subdivision) {
        // The maximum value we can add without shoving a time into the next slot.
        static constexpr auto nudge = te::TimeDuration::fromSeconds(0.05 / 96000.0);

        const auto ctx = frameContextAt(t, ts);
        auto frame = roundToInt(ctx.frame);
        auto labelTime = t;

        if (frame >= ctx.frameCount) {
            frame = 0;
            labelTime = ctx.end;
        }

        const auto barsBeats = ts.toBarsAndBeats(labelTime + nudge);
        return String::formatted("%d.%d.%02d", barsBeats.bars + 1, barsBeats.getWholeBeats() + 1, frame);
    }

    return lvl.snap.getTimecodeString(t, ts, false).replace("|", ".").replace("Bar ", "");
}

String SnapLadder::getPositionLabel(te::TimePosition t, const te::TempoSequence& ts) const {
    return getLabel(0, roundNearest(0, t, ts), ts);
}

juce::Optional<te::TimePosition> SnapLadder::parsePosition(const String& text, const te::TempoSequence& ts) const {
    if (mode_ == TimecodeDisplayMode::framesOnly) {
        const auto frames = text.trim();
        if (frames.isEmpty() || ! frames.containsOnly("0123456789"))
            return {};
        return te::TimePosition::fromSeconds(frames.getIntValue() / fps_);
    }

    const bool musical = mode_ == TimecodeDisplayMode::barsBeatsFrames;
    auto parts = StringArray::fromTokens(text, musical ? ".|:, " : ": ", "");
    parts.removeEmptyStrings();

    if (! musical && parts.size() == 1 && parts[0].containsOnly("0123456789.") && parts[0].containsAnyOf("0123456789"))
        return te::TimePosition::fromSeconds(parts[0].getDoubleValue());

    if (parts.isEmpty() || parts.size() > 3)
        return {};
    for (const auto& p : parts)
        if (! p.containsOnly("0123456789"))
            return {};

    if (musical) {
        const int bar = std::max(1, parts[0].getIntValue());
        const int beat = parts.size() > 1 ? std::max(1, parts[1].getIntValue()) : 1;
        const int frame = parts.size() > 2 ? parts[2].getIntValue() : 0;
        const auto beatStart = ts.toTime(te::tempo::BarsAndBeats { bar - 1, te::BeatDuration::fromBeats(beat - 1) });
        return frameToTime(frameContextAt(beatStart, ts), frame);
    }

    // Clock parts read right to left: frames, seconds, minutes.
    double seconds = parts[parts.size() - 1].getIntValue() / fps_;
    double scale = 1.0;
    for (int i = parts.size() - 2; i >= 0; --i, scale *= 60.0)
        seconds += parts[i].getIntValue() * scale;
    return te::TimePosition::fromSeconds(seconds);
}

}  // namespace MoTool
