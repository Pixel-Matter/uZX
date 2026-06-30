#include "TimelineGrid.h"
#include "../common/LookAndFeel.h"
#include "../../models/EditUtilities.h"
#include "../../controllers/App.h"

using namespace std::literals;
using namespace juce;

namespace MoTool {

namespace {

int chooseNiceFrameMultiple(double pixelsPerFrame, double minPixels) {
    static constexpr std::array multiples { 1, 2, 5, 10, 20, 25, 50, 100, 200, 500, 1000 };

    for (auto multiple : multiples) {
        if ((double)multiple * pixelsPerFrame >= minPixels)
            return multiple;
    }

    return multiples.back();
}

bool isCoarseFrameFarEnoughFromBeatEdges(int frame, int framesInBeat, double pixelsPerFrame) {
    static constexpr double minPixelsFromBeatEdge = 72.0;
    return (double)frame * pixelsPerFrame >= minPixelsFromBeatEdge
           && (double)(framesInBeat - frame) * pixelsPerFrame >= minPixelsFromBeatEdge;
}

}  // namespace

//==============================================================================
TimelineGrid::TimelineGrid(EditViewState& evs)
    : te::TempoSequence::Listener {evs.edit.tempoSequence}
    , editViewState(evs)
{
    editViewState.zoom.addListener(this);
    editViewState.edit.state.addListener(this);
}

TimelineGrid::~TimelineGrid() {
    editViewState.zoom.removeListener(this);
    editViewState.edit.state.removeListener(this);
}

std::vector<MoLookAndFeel::TimelineGridTick> TimelineGrid::getTicks() {
    if (ticksCacheValid)
        return ticksCache;

    auto newTicks = makeTicks();
    ticksCache.swap(newTicks);
    ticksCacheValid = true;
    return ticksCache;
}

void TimelineGrid::addListener(Listener* l) { listeners.add(l); }
void TimelineGrid::removeListener(Listener* l) { listeners.remove(l); }

void TimelineGrid::invalidateAndNotify() {
    ticksCacheValid = false;
    listeners.call(&TimelineGrid::Listener::gridChanged);
}

std::vector<MoLookAndFeel::TimelineGridTick>
TimelineGrid::makeTicksForSnaps(const std::vector<te::TimecodeSnapType>& snaps) {
    std::vector<MoLookAndFeel::TimelineGridTick> ticks;

    if (snaps.empty())
        return ticks;

    auto range = editViewState.zoom.getRange();
    auto time = range.getStart();
    auto endTime = range.getEnd();

    const auto& ts = editViewState.edit.tempoSequence;
    const auto& tempo = ts.getTempoAt(editViewState.edit.getTransport().getPosition());
    auto colorOffset = 3 - snaps.size();

    while (time < endTime) {
        size_t tickLevel = 0;
        time = snaps[tickLevel].roundTimeUp(time, ts);
        auto halfStep = snaps[tickLevel].getApproxIntervalTime(tempo, ts.isTripletsAtTime(time)) / 2.0;

        for (size_t i = 1; i < snaps.size(); ++i) {
            if (approximatelyEqual(time, snaps[i].roundTimeNearest(time, ts))) {
                tickLevel = i;
            }
        }

        String label;
        if (tickLevel != 0) {
            // only label coarse and coarser ticks
            label = snaps[tickLevel].getTimecodeString(time, ts, false);
            label = label.replace("|", ".").replace("Bar ", "");
        }

        auto x = roundToInt(editViewState.zoom.timeToX(time));
        ticks.push_back({ x, colorOffset + tickLevel, label });

        time = time + halfStep;
    }

    return ticks;
}

std::vector<MoLookAndFeel::TimelineGridTick>
TimelineGrid::makeExtendedFrameTicks(const TimecodeDisplayFormatExt& tcf) {
    std::vector<MoLookAndFeel::TimelineGridTick> ticks;

    auto range = editViewState.zoom.getRange();
    auto time = range.getStart();
    auto endTime = range.getEnd();

    const auto& ts = editViewState.edit.tempoSequence;
    const auto fps = (double)tcf.getFPS();
    const auto framesPerSecond = roundToInt(fps);
    const auto secondsPerFrame = 1.0 / fps;
    const auto pixelsPerFrame = secondsPerFrame / editViewState.zoom.getTimePerPixel().inSeconds();

    const auto fineFrameMultiple = chooseNiceFrameMultiple(pixelsPerFrame, 12.0);
    const auto coarseFrameMultiple = chooseNiceFrameMultiple(pixelsPerFrame, 48.0);
    const auto firstFrameInRange = std::max(0.0, time.inSeconds() * fps);
    const auto endFrame = std::max(0, (int)std::ceil(endTime.inSeconds() * fps - 1.0e-9));
    auto frame = std::max(0, (int)std::ceil((firstFrameInRange - 1.0e-9) / fineFrameMultiple) * fineFrameMultiple);

    for (; frame <= endFrame; frame += fineFrameMultiple) {
        auto tickTime = te::TimePosition::fromSeconds((double)frame / fps);

        if (tickTime < time)
            continue;
        if (tickTime >= endTime)
            break;

        size_t level = 0;
        if (frame % framesPerSecond == 0)
            level = 2;
        else if (frame % coarseFrameMultiple == 0)
            level = 1;

        String label;
        if (level != 0)
            label = tcf.getString(ts, tickTime, false);

        const auto x = roundToInt(editViewState.zoom.timeToX(tickTime));
        ticks.push_back({ x, level, label });
    }

    return ticks;
}

std::vector<MoLookAndFeel::TimelineGridTick>
TimelineGrid::makeBarsBeatsFrameTicks(const TimecodeDisplayFormatExt& tcf) {
    std::vector<MoLookAndFeel::TimelineGridTick> ticks;

    auto range = editViewState.zoom.getRange();
    auto time = range.getStart();
    auto endTime = range.getEnd();

    const auto& ts = editViewState.edit.tempoSequence;
    const auto& tempo = ts.getTempoAt(editViewState.edit.getTransport().getPosition());
    auto snaps = tcf.getOptimalSnapTypes(tempo, editViewState.zoom.getTimePerPixel(), ts.isTripletsAtTime(time));

    if (snaps.empty())
        return ticks;

    if (snaps.front().getLevel() >= 9)
        return makeTicksForSnaps(snaps);

    const auto fps = (double)tcf.getFPS();
    const auto secondsPerFrame = 1.0 / fps;
    const auto pixelsPerFrame = secondsPerFrame / editViewState.zoom.getTimePerPixel().inSeconds();

    const auto fineFrameMultiple = chooseNiceFrameMultiple(pixelsPerFrame, 12.0);
    const auto coarseFrameMultiple = chooseNiceFrameMultiple(pixelsPerFrame, 48.0);
    const auto beatRange = ts.toBeats(range);
    const auto firstBeat = std::max(0, (int)std::floor(beatRange.getStart().inBeats()));
    const auto lastBeat = std::max(firstBeat, (int)std::ceil(beatRange.getEnd().inBeats()));

    for (int beat = firstBeat; beat <= lastBeat; ++beat) {
        const auto beatStart = ts.toTime(te::BeatPosition::fromBeats((double)beat));
        const auto beatEnd = ts.toTime(te::BeatPosition::fromBeats((double)beat + 1.0));

        if (beatEnd < time || beatStart >= endTime)
            continue;

        const auto beatLength = beatEnd - beatStart;
        const auto framesInBeat = std::max(1, (int)std::ceil(beatLength.inSeconds() * fps - 1.0e-9));
        const auto firstFrameInRange = std::max(0.0, ((time - beatStart).inSeconds() * fps));
        auto frame = std::max(0, (int)std::ceil((firstFrameInRange - 1.0e-9) / fineFrameMultiple) * fineFrameMultiple);

        for (; frame < framesInBeat; frame += fineFrameMultiple) {
            auto tickTime = beatStart + te::TimeDuration::fromSeconds((double)frame / fps);

            if (tickTime < time)
                continue;
            if (tickTime >= endTime || tickTime >= beatEnd)
                break;

            size_t level = 0;
            String label;

            if (frame == 0) {
                for (size_t i = 1; i < snaps.size(); ++i) {
                    if (snaps[i].getLevel() >= 9
                        && approximatelyEqual(tickTime, snaps[i].roundTimeNearest(tickTime, ts))) {
                        level = i;
                    }
                }

                if (level != 0)
                    label = snaps[level].getTimecodeString(tickTime, ts, false).replace("|", ".").replace("Bar ", "");
            } else if (frame % coarseFrameMultiple == 0
                       && isCoarseFrameFarEnoughFromBeatEdges(frame, framesInBeat, pixelsPerFrame)) {
                level = 1;
                label = tcf.getString(ts, tickTime, false).replace("|", ".");
            }

            const auto x = roundToInt(editViewState.zoom.timeToX(tickTime));
            ticks.push_back({ x, level, label });
        }
    }

    return ticks;
}

std::vector<MoLookAndFeel::TimelineGridTick> TimelineGrid::makeTicks() {
    auto tcf = Helpers::getEditTimecodeFormat(editViewState.edit);
    if (tcf.isBarsBeatsFrames())
        return makeBarsBeatsFrameTicks(tcf);
    if (tcf.isExtendedFramesOnly())
        return makeExtendedFrameTicks(tcf);

    auto time = editViewState.zoom.getRange().getStart();

    // TODO iterate tempo setting along the whole time span and regular grid inbetween
    const auto& ts = editViewState.edit.tempoSequence;
    const auto& tempo = ts.getTempoAt(editViewState.edit.getTransport().getPosition());

    auto snaps = tcf.getOptimalSnapTypes(tempo, editViewState.zoom.getTimePerPixel(), ts.isTripletsAtTime(time));
    // for (auto& snap : snaps) {
    //     DBG("Snap " << snap.getLevel()
    //         << ": " << snap.getDescription(tempo, ts.isTripletsAtTime(time))
    //         << ", tc " << snap.getTimecodeString(0s, ts, false)
    //     );
    // }

    return makeTicksForSnaps(snaps);
}

void TimelineGrid::zoomChanged() {
    ticksCacheValid = false;
}

void TimelineGrid::valueTreePropertyChanged(ValueTree&, const Identifier& property) {
    if (property == te::IDs::timecodeFormat)
        invalidateAndNotify();
}

void TimelineGrid::selectableObjectChanged(te::Selectable*) {
    invalidateAndNotify();
}

}  // namespace MoTool
