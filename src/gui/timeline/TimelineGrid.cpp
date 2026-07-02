#include "TimelineGrid.h"
#include "../common/LookAndFeel.h"
#include "../../models/EditUtilities.h"
#include "../../models/Ids.h"
#include "../../models/SnapLadder.h"

using namespace std::literals;
using namespace juce;

namespace MoTool {

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

std::vector<MoLookAndFeel::TimelineGridTick> TimelineGrid::makeTicks() {
    std::vector<MoLookAndFeel::TimelineGridTick> ticks;

    auto& edit = editViewState.edit;
    const auto& ts = edit.tempoSequence;
    const auto& tempo = ts.getTempoAt(edit.getTransport().getPosition());
    const auto range = editViewState.zoom.getRange();
    const auto timePerPixel = editViewState.zoom.getTimePerPixel();
    const bool triplets = ts.isTripletsAtTime(range.getStart());

    const auto mode = Helpers::getTimecodeDisplayMode(edit);
    const auto fps = Helpers::getProjectFps(edit);
    const auto framesPerBeat = jmax(1, roundToInt(editViewState.getCurrentFramesPerBeat()));

    const SnapLadder ladder(mode, fps, framesPerBeat, Helpers::getGridSubdivisionPattern(edit));

    // Three display levels, finest first: short unlabeled ticks, labeled medium
    // ticks, and the next structural milestone (beat, bar, bar group, ...).
    const int fine = ladder.getBestLevel(timePerPixel, 12.0, tempo, triplets);
    const int medium = ladder.getBestLevel(timePerPixel, 48.0, tempo, triplets);
    const int coarse = ladder.getMilestoneAfter(medium);

    std::vector<int> displayLevels { fine };
    if (medium != fine)
        displayLevels.push_back(medium);
    if (coarse != displayLevels.back())
        displayLevels.push_back(coarse);
    while (displayLevels.size() < 3) {
        const auto milestone = ladder.getMilestoneAfter(displayLevels.back());
        if (milestone == displayLevels.back())
            break;
        displayLevels.push_back(milestone);
    }

    const auto visualOffset = 3 - displayLevels.size();
    const auto mediumIndex = medium == fine ? (size_t) 0 : (size_t) 1;

    const int beatLevel = ladder.getBeatLevel();
    const auto pixelsFromNearestBeat = [&](te::TimePosition t) {
        if (beatLevel < 0)
            return std::numeric_limits<double>::max();
        const auto beatTime = ladder.roundNearest(beatLevel, t, ts);
        return std::abs((t - beatTime).inSeconds()) / timePerPixel.inSeconds();
    };

    auto time = jmax(te::TimePosition(), ladder.roundUp(fine, range.getStart(), ts));

    while (time < range.getEnd()) {
        // The coarsest display level this tick lies on.
        size_t matched = 0;
        for (size_t i = displayLevels.size(); --i > 0;) {
            if (ladder.isOnGrid(displayLevels[i], time, ts)) {
                matched = i;
                break;
            }
        }

        String label;
        if (matched >= mediumIndex) {
            const auto level = displayLevels[matched];
            const auto kind = ladder.getKind(level);
            // Frame tick labels too close to a beat boundary would crowd its label.
            const bool crowded = (kind == SnapLadder::LevelKind::frames
                                  || kind == SnapLadder::LevelKind::subdivision)
                                 && pixelsFromNearestBeat(time) < 72.0;
            if (! crowded)
                label = ladder.getLabel(level, time, ts);
        }

        const auto x = roundToInt(editViewState.zoom.timeToX(time));
        ticks.push_back({ x, visualOffset + matched, label });

        const auto next = ladder.getNextTick(fine, time, ts);
        if (next <= time) {
            jassertfalse;
            break;
        }
        time = next;
    }

    return ticks;
}

void TimelineGrid::zoomChanged() {
    ticksCacheValid = false;
}

void TimelineGrid::valueTreePropertyChanged(ValueTree&, const Identifier& property) {
    if (property == IDs::timecodeDisplayMode
        || property == IDs::projectFps
        || property == IDs::gridSubdivision
        || property == te::IDs::timecodeFormat)
        invalidateAndNotify();
}

void TimelineGrid::selectableObjectChanged(te::Selectable*) {
    invalidateAndNotify();
}

}  // namespace MoTool
