#pragma once

#include <JuceHeader.h>

#include "TrackComponents.h"
#include "DetailsPanelComponent.h"
#include "Ruler.h"
#include "PlayheadComponent.h"
#include "TimelineGrid.h"

namespace MoTool {

//==============================================================================
/**
 * Main component for the timeline edit view, containing tracks, ruler, and playhead
 */
class EditComponent final : public Component,
                      private FlaggedAsyncUpdater,  // for marking and updating asynchronously
                      private ValueTree::Listener
                    {
public:
    EditComponent(te::Edit&, EditViewState&);
    ~EditComponent() override;

private:
    void valueTreePropertyChanged(ValueTree&, const Identifier&) override;

    void handleAsyncUpdate() override;
    void resized() override;
    void paint(Graphics& g) override;

    te::Edit& edit;
    EditViewState& editViewState;
    TimelineGrid grid {editViewState};

    PlayheadComponent playhead {edit, editViewState};
    RulerComponent ruler {edit, editViewState, grid};
    TracksContainerComponent tracksContainer {edit, editViewState, grid};
    Viewport trackViewport;
    DetailsPanelComponent detailsPanel {editViewState, grid};

    bool updateSizes = false;
};

}
