#pragma once

#include <JuceHeader.h>

#include "TrackComponents.h"
#include "DetailsPanelComponent.h"
#include "Ruler.h"
#include "PlayheadComponent.h"
#include "TimelineGrid.h"

namespace MoTool {

struct EditComponentOptions {
    bool showDetailsPanel = true;
};

//==============================================================================
/**
 * Main component for the timeline edit view, containing tracks, ruler, and playhead
 */
class EditComponent final : public Component,
                      private FlaggedAsyncUpdater,  // for marking and updating asynchronously
                      private ValueTree::Listener
                    {
public:
    EditComponent(te::Edit&, EditViewState&, EditComponentOptions opts = {});
    ~EditComponent() override;

private:
    void valueTreePropertyChanged(ValueTree&, const Identifier&) override;

    void handleAsyncUpdate() override;
    void resized() override;
    void paint(Graphics& g) override;

    EditComponentOptions options;
    te::Edit& edit;
    EditViewState& editViewState;
    TimelineGrid grid {editViewState};

    class ZoomableViewport : public Viewport {
    public:
        ZoomableViewport(EditViewState& evs) : editViewState(evs) {}
        void mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel) override;
    private:
        EditViewState& editViewState;
    };

    PlayheadComponent playhead {edit, editViewState};
    RulerComponent ruler {edit, editViewState, grid};
    TracksContainerComponent tracksContainer {edit, editViewState, grid};
    ZoomableViewport trackViewport {editViewState};
    DetailsPanelComponent detailsPanel {editViewState, grid};

    bool updateSizes = false;
};

}
