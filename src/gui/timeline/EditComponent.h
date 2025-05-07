#pragma once

#include <JuceHeader.h>

#include "../common/Components.h"
#include "TrackComponents.h"
#include "DetailsPanelComponent.h"
#include "Ruler.h"

namespace MoTool {

//==============================================================================
class EditComponent final : public Component,
                      //   private ApplicationCommandTarget
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

    PlayheadComponent playhead {edit, editViewState};
    RulerComponent ruler {edit, editViewState};
    TracksContainerComponent tracksContainer {edit, editViewState, ruler};
    Viewport trackViewport;
    DetailsPanelComponent detailsPanel {editViewState};

    bool updateSizes = false;
};

}
