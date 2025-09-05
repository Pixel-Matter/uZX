#include "TimelineGrid.h"
#include "../common/LookAndFeel.h"
#include "../../models/EditUtilities.h"
#include "../../controllers/App.h"
#include "juce_core/juce_core.h"

using namespace std::literals;
using namespace juce;

namespace MoTool {

//==============================================================================
TimelineGrid::TimelineGrid(EditViewState& evs)
    : editViewState(evs)
{
    editViewState.zoom.addListener(this);
}

TimelineGrid::~TimelineGrid() {
    editViewState.zoom.removeListener(this);
}

std::vector<MoLookAndFeel::TimelineGridTick> TimelineGrid::getTicks() {
    if (ticksCacheValid)
        return ticksCache;

    auto newTicks = makeTicks();
    ticksCache.swap(newTicks);
    ticksCacheValid = true;
    return ticksCache;
}

std::vector<MoLookAndFeel::TimelineGridTick> TimelineGrid::makeTicks() {
    std::vector<MoLookAndFeel::TimelineGridTick> ticks;

    auto tcf = Helpers::getEditTimecodeFormat(editViewState.edit);
    auto range = editViewState.zoom.getRange();
    auto time = range.getStart();
    auto endTime = range.getEnd();

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

    // prepare up to 3 levels of snap if available: finest then coarse and coarser
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

void TimelineGrid::zoomChanged() {
    ticksCacheValid = false;
}

}  // namespace MoTool