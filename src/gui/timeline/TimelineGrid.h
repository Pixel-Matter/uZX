#pragma once

#include <JuceHeader.h>
#include "../../controllers/EditState.h"
#include "../common/LookAndFeel.h"

namespace MoTool {

struct TimecodeDisplayFormatExt;

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

    void addListener(Listener* l);
    void removeListener(Listener* l);

private:
    std::vector<MoLookAndFeel::TimelineGridTick> makeTicks();
    std::vector<MoLookAndFeel::TimelineGridTick>
        makeTicksForSnaps(const std::vector<te::TimecodeSnapType>&);
    std::vector<MoLookAndFeel::TimelineGridTick>
        makeExtendedFrameTicks(const TimecodeDisplayFormatExt&);
    std::vector<MoLookAndFeel::TimelineGridTick>
        makeBarsBeatsFrameTicks(const TimecodeDisplayFormatExt&);
    void invalidateAndNotify();

    void zoomChanged() override;
    void valueTreePropertyChanged(ValueTree&, const Identifier&) override;
    void selectableObjectChanged(te::Selectable*) override;

    std::atomic<bool> ticksCacheValid { false };
    std::vector<MoLookAndFeel::TimelineGridTick> ticksCache;
    EditViewState& editViewState;
    ListenerList<Listener> listeners;
};

}  // namespace MoTool
