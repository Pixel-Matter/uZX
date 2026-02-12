#pragma once

#include <JuceHeader.h>
#include "../../controllers/EditState.h"
#include "../common/LookAndFeel.h"

namespace MoTool {

//==============================================================================
class TimelineGrid : private ZoomViewState::Listener,
                     private ValueTree::Listener {
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
    void invalidateAndNotify();

    void zoomChanged() override;
    void valueTreePropertyChanged(ValueTree&, const Identifier&) override;

    std::atomic<bool> ticksCacheValid { false };
    std::vector<MoLookAndFeel::TimelineGridTick> ticksCache;
    EditViewState& editViewState;
    ListenerList<Listener> listeners;
};

}  // namespace MoTool
