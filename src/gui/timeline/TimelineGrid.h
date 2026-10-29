#pragma once

#include <JuceHeader.h>
#include "../../controllers/EditState.h"
#include "../../models/SnapLadder.h"
#include "../common/LookAndFeel.h"

namespace MoTool {

//==============================================================================
class TimelineGrid : private ZoomViewState::Listener,
                     private ValueTree::Listener,
                     private te::TempoSequence::Listener {
public:

    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void gridChanged() = 0;
    };

    TimelineGrid(EditViewState& evs);

    ~TimelineGrid() override;

    std::vector<MoLookAndFeel::TimelineGridTick> getTicks();
    te::TimePosition snapTimeToVisibleGrid(te::TimePosition time) const;
    /** Where a click or drag at timeline x puts the playhead: not before 0, and on the visible grid when
        snapping is on. */
    te::TimePosition playheadTimeAtX(int x) const;

    void addListener(Listener* l);
    void removeListener(Listener* l);

private:
    SnapLadder makeSnapLadder() const;
    int getVisibleSnapLevel(const SnapLadder& ladder) const;
    std::vector<MoLookAndFeel::TimelineGridTick> makeTicks();
    void invalidateAndNotify();

    void viewRangeChanged() override;
    void valueTreePropertyChanged(ValueTree&, const Identifier&) override;
    void selectableObjectChanged(te::Selectable*) override;

    std::atomic<bool> ticksCacheValid { false };
    std::vector<MoLookAndFeel::TimelineGridTick> ticksCache;
    EditViewState& editViewState;
    ListenerList<Listener> listeners;
};

}  // namespace MoTool
