#pragma once

#include <JuceHeader.h>
#include "../../controllers/EditState.h"
#include "../common/LookAndFeel.h"

namespace MoTool {

//==============================================================================
class TimelineGrid : private ZoomViewState::Listener {
public:
    TimelineGrid(EditViewState& evs);

    ~TimelineGrid() override;

    std::vector<MoLookAndFeel::TimelineGridTick> getTicks();

private:
    std::vector<MoLookAndFeel::TimelineGridTick> makeTicks();

    void zoomChanged() override;

    std::atomic<bool> ticksCacheValid { false };
    std::vector<MoLookAndFeel::TimelineGridTick> ticksCache;
    EditViewState& editViewState;
};

}  // namespace MoTool